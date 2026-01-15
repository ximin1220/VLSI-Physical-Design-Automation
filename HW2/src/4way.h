#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <climits>
#include <time.h>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>

// ===== 帶比例參數的可行性檢查（4-way 用）=====
bool feasible_r(int cell_idx, Instance &inst, double lower_ratio, double upper_ratio)
{
    CELL &cell = inst.cells[cell_idx];
    if (cell.locked)
        return false;

    long long A_size = inst.A_size;
    long long B_size = inst.B_size;

    double lower_bound = lower_ratio * inst.total_size;
    double upper_bound = upper_ratio * inst.total_size;

    if (cell.group == 0)
    { // A -> B
        if ((A_size - cell.size) < lower_bound)
            return false;
        if ((B_size + cell.size) > upper_bound)
            return false;
    }
    else
    { // B -> A
        if ((B_size - cell.size) < lower_bound)
            return false;
        if ((A_size + cell.size) > upper_bound)
            return false;
    }
    return true;
}

int pop_best_feasible_r(Bucket &B, Instance &inst, double lower_ratio, double upper_ratio)
{
    while (B.top_bucket() > -1)
    {
        int i = B.top_bucket();
        auto &lst = B.bins[i];
        for (auto it = lst.begin(); it != lst.end(); ++it)
        {
            int u = *it;
            if (feasible_r(u, inst, lower_ratio, upper_ratio))
            {
                lst.erase(it);
                if (i == B.cur && lst.empty()) {
                    B.dec_to_non_empty();
                }
                return u; 
            }
        }
        int has_next = B.dec_to_non_empty();
        if (has_next == -1)
            break;
    }
    return -1;
}

/**
 * @brief 4-way 用的 *多 Pass* FM (Optimized "Replay" Version)
 */
void FM_r_optimized(Instance &inst, double lower_ratio, double upper_ratio)
{
    bool improvement_found_in_pass = true;

    Bucket bucket(inst.maxp);
    for (auto &cell : inst.cells)
    {
        if(cell.locked) cell.locked = false;
        bucket.insert(cell);
    }

    while (improvement_found_in_pass) // Loop over passes
    {
        improvement_found_in_pass = false;
        
        const vector<CELL> initial_cells = inst.cells;
        const long long initial_A_size = inst.A_size;
        const long long initial_B_size = inst.B_size;
        const long long initial_cutsize = inst.cutsize;

        long long best_cutsize_in_pass = initial_cutsize;
        long long current_pass_cutsize = initial_cutsize;
        int best_step = -1; 

        vector<int> move_history;
        int num_unlocked = 0;
        for(auto& c : initial_cells) num_unlocked += !c.locked;
        move_history.reserve(num_unlocked);

        Bucket pass_bucket_sim = bucket; 
        Instance pass_inst_sim = inst; 

        for (int i = 0; i < num_unlocked; ++i)
        {
            // *** 關鍵差異：使用 _r 版本的 feasible check ***
            int to_move = pop_best_feasible_r(pass_bucket_sim, pass_inst_sim, lower_ratio, upper_ratio); 
            
            if (to_move == -1) break; 

            int move_gain = pass_inst_sim.cells[to_move].gain;
            move_history.push_back(to_move);

            update_gain(to_move, pass_inst_sim, pass_bucket_sim);
            
            current_pass_cutsize -= move_gain; 
            
            if (current_pass_cutsize < best_cutsize_in_pass)
            {
                best_cutsize_in_pass = current_pass_cutsize;
                best_step = i;
            }
        }

        // --- Pass 結束，檢查並恢復 ---
        
        if (best_step != -1 && best_cutsize_in_pass < initial_cutsize)
        {
            improvement_found_in_pass = true;
            
            inst.cells = initial_cells; 
            inst.A_size = initial_A_size;
            inst.B_size = initial_B_size;
            
            compute_cutsize(inst); 
            compute_gains(inst);
            bucket = Bucket(inst.maxp); 
            for(auto& cell : inst.cells) bucket.insert(cell);

            long long current_cutsize = initial_cutsize;
            for (int i = 0; i <= best_step; ++i)
            {
                int to_move = move_history[i];
                CELL &cell_to_move = inst.cells[to_move];
                
                int move_gain_at_replay = cell_to_move.gain;
                bucket.erase(to_move, cell_to_move.gain, cell_to_move.it);
                
                update_gain(to_move, inst, bucket); 
                current_cutsize -= move_gain_at_replay;
            }
            inst.cutsize = current_cutsize;

            // --- 準備下一個 Pass (for this sub-problem) ---
            for (auto &c : inst.cells) c.locked = false;
            compute_cutsize(inst);
            compute_gains(inst);
            bucket = Bucket(inst.maxp);
            for (auto &cell : inst.cells) bucket.insert(cell);
        }
        else
        {
            inst.cells = initial_cells;
            inst.A_size = initial_A_size;
            inst.B_size = initial_B_size;
            inst.cutsize = initial_cutsize;
            
            improvement_found_in_pass = false; 
            
            for (auto &c : inst.cells) c.locked = false;
        }
    } // end while(passes)
}


// 2-way 頂層呼叫（呼叫多 Pass FM）
void run_2way_once_legacy(Instance &inst, double lower_ratio, double upper_ratio)
{
    auto sums = new_initial_partition_2way(inst, lower_ratio, upper_ratio);
    inst.A_size = sums.first;
    inst.B_size = sums.second;

    compute_cutsize(inst);
    compute_gains(inst);

    FM(inst); // ← 呼叫新的多 pass FM
    
    for (auto &c : inst.cells)
        c.locked = false;
}

// 4-way 子問題用（呼叫多 Pass FM_r）
void run_2way_once(Instance &inst, double lower_ratio, double upper_ratio)
{
    auto sums = new_initial_partition_2way(inst, lower_ratio, upper_ratio);
    inst.A_size = sums.first;
    inst.B_size = sums.second;

    compute_cutsize(inst);
    compute_gains(inst);

    FM_r_optimized(inst, lower_ratio, upper_ratio); // ← 執行新的 *多 Pass* FM_r
    
    for (auto &c : inst.cells)
        c.locked = false;
}

vector<int> collect_group_cells(const Instance &inst, int g)
{
    vector<int> res;
    res.reserve(inst.cells.size());
    for (const auto &c : inst.cells)
        if (c.group == g)
            res.push_back(c.idx);
    return res;
}

void build_subinstance(
    const Instance &root,
    const vector<int> &keep_cells,
    Instance &sub,
    vector<int> &sub2orig,
    vector<int> &orig2sub)
{
    orig2sub.assign(root.cells.size(), -1);
    sub2orig.clear();
    sub.cells.clear();
    sub.nets.clear();

    sub.cells.reserve(keep_cells.size());
    for (int i = 0; i < (int)keep_cells.size(); ++i)
    {
        int orig_idx = keep_cells[i];
        orig2sub[orig_idx] = i;

        CELL c = root.cells[orig_idx];
        c.idx = i;
        c.gain = 0;
        c.locked = false;
        c.group = 0; 
        c.nets_arr.clear();
        sub.cells.push_back(c);
        sub2orig.push_back(orig_idx);
    }

    for (const auto &net : root.nets)
    {
        vector<int> new_cells;
        new_cells.reserve(net.cells_arr.size());
        for (int u : net.cells_arr)
        {
            int v = orig2sub[u];
            if (v != -1)
                new_cells.push_back(v);
        }
        if (new_cells.empty())
            continue;
        NET n;
        n.name = net.name;
        n.idx = (int)sub.nets.size();
        n.num_cell = (int)new_cells.size();
        n.is_cut = false;
        n.A_num = n.B_num = 0;
        n.critical = false;
        n.cells_arr = move(new_cells);
        sub.nets.push_back(move(n));
    }
    for (const auto &n : sub.nets)
        for (int u : n.cells_arr)
            sub.cells[u].nets_arr.push_back(n.idx);

    sub.total_size = 0;
    int mxdeg = 0;
    for (auto &c : sub.cells)
    {
        sub.total_size += c.size;
        mxdeg = max(mxdeg, (int)c.nets_arr.size());
    }
    sub.maxp = mxdeg;
    sub.A_size = sub.B_size = 0;
    sub.cutsize = 0;
}

void map_back_groups(
    Instance &root,
    const Instance &sub,
    const vector<int> &sub2orig,
    int label_if_sub0,
    int label_if_sub1)
{
    for (const auto &c : sub.cells)
    {
        int orig_idx = sub2orig[c.idx];
        root.cells[orig_idx].group = (c.group == 0 ? label_if_sub0 : label_if_sub1);
    }
}

void partition_4way(Instance &root)
{
    // 第 1 層：root 上先 2-way
    run_2way_once_legacy(root, 0.48, 0.52); // 呼叫多 Pass FM

    // group=0 → split 成 {0,2}
    {
        vector<int> g0 = collect_group_cells(root, 0);
        if (!g0.empty())
        {
            Instance subA;
            vector<int> sub2orig;
            vector<int> orig2sub;
            build_subinstance(root, g0, subA, sub2orig, orig2sub);

            double x = (double)subA.total_size / (double)root.total_size;
            double lower2 = max(0.0, 0.225 / x);
            double upper2 = min(1.0, 0.275 / x);

            run_2way_once(subA, lower2, upper2); // 呼叫 *多 Pass* FM_r
            map_back_groups(root, subA, sub2orig, 0, 2);
        }
    }
    // group=1 → split 成 {1,3}
    {
        vector<int> g1 = collect_group_cells(root, 1);
        if (!g1.empty())
        {
            Instance subB;
            vector<int> sub2orig;
            vector<int> orig2sub;
            build_subinstance(root, g1, subB, sub2orig, orig2sub);

            double x = (double)subB.total_size / (double)root.total_size;
            double lower2 = max(0.0, 0.225 / x);
            double upper2 = min(1.0, 0.275 / x);

            run_2way_once(subB, lower2, upper2); // 呼叫 *多 Pass* FM_r
            map_back_groups(root, subB, sub2orig, 1, 3);
        }
    }
}