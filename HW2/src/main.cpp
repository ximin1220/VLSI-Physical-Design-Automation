// main.cpp
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
#include <queue>
#include <cmath>
#include "inst.h"
#include "parse.h"
#include "initial_partition.h"
#include "bucket.h"
#include "write.h"
#include <iomanip> // 為了 setprecision

using namespace std;

void compute_cutsize(Instance &inst);
void compute_gains(Instance &inst);
void FM(Instance &inst); // Bucket is managed internally
void update_gain(int moved_cell_idx, Instance &inst, Bucket &bucket);
#include "4way.h"
Instance inst;
map<int, vector<int>> buckets; 
vector<int> A, B; 
void put_in_buckets(Instance &inst); 

/* ================
 * 主程式
 * ================ */
int main(int argc, char **argv)
{
    if (argc < 4) 
    {
        cerr << "Usage: ./hw2 <input.txt> <output.txt> <k=2|4>\n\n";
        return 1;
    }
    std::string in = argv[1];
    std::string out = argv[2];
    int k = std::atoi(argv[3]);
    double lower = (k == 2) ? 0.45 : 0.225;
    double upper = (k == 2) ? 0.55 : 0.275;

    if (!parseInput(in, inst))
        return 2;
    if (k == 2)
    {
        auto sums = new_initial_partition_2way(inst, lower, upper);
        inst.A_size = sums.first;
        inst.B_size = sums.second;
        cout << fixed << setprecision(3);
        cout << "TotalSize = " << inst.total_size << "\n";
        cout << "GroupA Size = " << sums.first
             << " (" << (double)sums.first / (double)inst.total_size << ")\n";
        cout << "GroupB Size = " << sums.second
             << " (" << (double)sums.second / (double)inst.total_size << ")\n";

        compute_cutsize(inst);
        compute_gains(inst);

        // Bucket 不在這裡建立
        FM(inst); // <--- 呼叫新的、多 Pass、高效能的 FM
        
        cout << "Final Cutsize: " << inst.cutsize << "\n";

        writeOutput(inst, out, true, true);
    }
    else
    {
        partition_4way(inst);
        writeOutput4way(inst, out);
    }

    return 0;
}

void compute_cutsize(Instance &inst)
{
    int cutsize = 0;
    for (auto &net : inst.nets)
    {
        net.A_num = net.B_num = 0;
        for (int cidx : net.cells_arr)
        {
            if (inst.cells[cidx].group == 0)
                net.A_num++;
            else
                net.B_num++;
        }
        net.is_cut = (net.A_num > 0 && net.B_num > 0);
        net.critical = (net.A_num == 1 || net.B_num == 1);
        if (net.is_cut)
            cutsize++;
    }

    inst.cutsize = cutsize;
    cout << "Computed Cutsize: " << cutsize << "\n"; 
}

void compute_gains(Instance &inst)
{
    for (auto &cell : inst.cells)
    {
        int F = 0, T = 0;
        for (int nid : cell.nets_arr)
        {
            NET &net = inst.nets[nid];
            int F_num = (cell.group == 0) ? net.A_num : net.B_num;
            int T_num = (cell.group == 0) ? net.B_num : net.A_num;

            if (F_num == 1) F++;
            if (T_num == 0) T++;
        }
        cell.gain = F - T; 
    }
}

void put_in_buckets(Instance &inst)
{
    buckets.clear();
    for (auto &cell : inst.cells)
    {
        buckets[cell.gain].push_back(cell.idx);
    }
}


/**
 * @brief 執行多 Pass 的 FM 演算法 (Optimized "Replay" Version)
 */
void FM(Instance &inst)
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
            int to_move = pop_best_feasible(pass_bucket_sim, pass_inst_sim); 
            
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

            cout << "Pass improvement: Cutsize = " << inst.cutsize << "\n";

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
            
            cout << "No improvement in this pass. FM terminates.\n";
            improvement_found_in_pass = false; 
            
            for (auto &c : inst.cells) c.locked = false;
        }
    } // end while(passes)
}


/**
 * @brief 更新 gain (核心邏輯) - *修正版*
 * 這是您最初的邏輯，它是正確的。
 */
void update_gain(int moved_cell_idx, Instance &inst, Bucket &bucket)
{
    CELL &moved_cell = inst.cells[moved_cell_idx];
    moved_cell.locked = true;
    int g_from = moved_cell.group;
    int g_to = 1 - g_from;
    // int move_gain = moved_cell.gain; // Not used here

    for (int nid : moved_cell.nets_arr)
    {
        NET &net = inst.nets[nid];
        
        int F_num = (g_from == 0) ? net.A_num : net.B_num;
        int T_num = (g_from == 0) ? net.B_num : net.A_num;

        // --- 這是原本正確的邏輯 ---
        if (T_num == 0) { // T=0, F=F_num
            // M 移過去 -> T=1, F=F_num-1. Net 變 cut
            for (int cidx : net.cells_arr) {
                CELL &cell = inst.cells[cidx];
                if (cell.locked || cidx == moved_cell_idx) continue;
                int new_gain = cell.gain + 1; // Gain++
                bucket.update(cell, new_gain);
            }
        } else if (T_num == 1) { // T=1, F=F_num
            // M 移過去 -> T=2, F=F_num-1. Net 仍 cut
            // 找到 T-side 唯一那顆
            for (int cidx : net.cells_arr) {
                if (inst.cells[cidx].group == g_to && !inst.cells[cidx].locked) {
                    int new_gain = inst.cells[cidx].gain - 1; // Gain-- (T=1 -> T=2)
                    bucket.update(inst.cells[cidx], new_gain);
                    break;
                }
            }
        }
        
        // *** M 已經移動 ***
        F_num--;
        T_num++;

        if (F_num == 0) { // T=T_num, F=0 (i.e., old F=1)
            // M 移走後 F=0, Net 變 not cut
            for (int cidx : net.cells_arr) {
                CELL &cell = inst.cells[cidx];
                if (cell.locked || cidx == moved_cell_idx) continue;
                // T-side 的 cell gain--
                int new_gain = cell.gain - 1;
                bucket.update(cell, new_gain);
            }
        } else if (F_num == 1) { // T=T_num, F=1 (i.e., old F=2)
            // M 移走後 F=1. Net 仍 cut
            // 找到 F-side 唯一那顆
            for (int cidx : net.cells_arr) {
                if (inst.cells[cidx].group == g_from && !inst.cells[cidx].locked) {
                    int new_gain = inst.cells[cidx].gain + 1; // Gain++
                    bucket.update(inst.cells[cidx], new_gain);
                    break;
                }
            }
        }
        // --- 邏輯結束 ---

        if (g_from == 0)
        {
            net.A_num--;
            net.B_num++;
        }
        else
        {
            net.B_num--;
            net.A_num++;
        }
        net.is_cut = (net.A_num > 0 && net.B_num > 0);
    }

    if (g_from == 0)
    {
        inst.A_size -= moved_cell.size;
        inst.B_size += moved_cell.size;
    }
    else
    {
        inst.B_size -= moved_cell.size;
        inst.A_size += moved_cell.size;
    }
    moved_cell.group = g_to;
}