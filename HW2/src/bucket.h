#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <climits>
#include <time.h>
#include <unordered_map>
#include <algorithm>
#include <list>
using namespace std;

struct Bucket {
    int Gmax;                   // 例如最大度數
    int offset;                 // = Gmax（把[-Gmax, Gmax] 映到 [0, 2*Gmax]）
    int cur;                    // 目前指向的最大非空桶 index
    vector<list<int>> bins;     // 每個 gain 一個 list，存 cell idx

    Bucket(int gmax=0): Gmax(gmax), offset(gmax), cur(-1), bins(2*gmax+1) {}

    inline int idx(int gain){
         return gain + offset; }

    void insert(CELL &cell) {
        if(cell.locked) return;
        int i = idx(cell.gain);
        bins[i].push_front(cell.idx);
        cell.it = bins[i].begin();
        cur = max(cur, i);
    }
    // 有了 cell→iterator 的表即可 O(1) 移除
    void erase(int cell, int gain, list<int>::iterator it) {
        bins[idx(gain)].erase(it);
        if (cur == idx(gain) && bins[cur].empty()) dec_to_non_empty();
    }
    int dec_to_non_empty() {
        while(1){
            cur--;
            // if(cur >= offset && !bins[cur].empty()) // OLD
            if(cur >= 0 && !bins[cur].empty()) // NEW: 允許負增益
                return cur;
            // if(cur < offset) break; // OLD
            if(cur < 0) break; // NEW: 檢查是否小於 0
        }
        return -1;
    }

    void update(CELL &cell, int new_gain) {
        // 從舊桶移除
        erase(cell.idx, cell.gain, cell.it);
        // 更新 gain
        cell.gain = new_gain;
        // 插入新桶
        insert(cell);
    }
    // 回傳目前最大 gain 的桶 index（沒有回 -1）
    int top_bucket() const { return cur; }
};

bool feasible(int cell_idx, Instance &inst){
    // return true;
    CELL &cell = inst.cells[cell_idx];
    if(cell.locked) return false;

    long long A_size = inst.A_size;
    long long B_size = inst.B_size;
    

    double lower_bound = 0.45 * inst.total_size;
    double upper_bound = 0.55 * inst.total_size;

    if(cell.group == 0){ // A -> B
        if((A_size - cell.size) < lower_bound) return false;
        if((B_size + cell.size) > upper_bound) return false;
    } else { // B -> A
        if((B_size - cell.size) < lower_bound) return false;
        if((A_size + cell.size) > upper_bound) return false;
    }

    return true;
}

// feasible(cell) 你檢查 ratio / locked 的函式
int pop_best_feasible(Bucket& B, Instance& inst) {
    // cout << B.top_bucket() << " " << B.offset << "\n"; // Debug
    // while (B.top_bucket() >= B.offset) { // OLD
    while (B.top_bucket() > -1) { // NEW: 搜尋所有 bucket，包含負增益
        int i = B.top_bucket();
        auto &lst = B.bins[i];
        // cout << inst.cells[lst.front()].name << " in bucket " << i - B.offset << "\n";
        // 在同一桶內從前面掃，找到第一個可行的
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            int u = *it;
            // cout << "Checking cell " << inst.cells[u].name << "\n";
            if (feasible(u, inst)) {
                // 找到了！將它從 bucket 移除並回傳
                lst.erase(it);
                // 檢查是否需要更新 cur
                if (i == B.cur && lst.empty()) {
                    B.dec_to_non_empty();
                }
                return u; // 回傳 cell idx
            }
        }
        // 這個桶沒有可行的，往下一個非空桶
        int has_next = B.dec_to_non_empty();
        // cout << "has_next: " << has_next << "\n";
        if(has_next == -1) break;
        // cout << B.top_bucket() << " " << B.offset << "\n"; // Debug
    }
    return -1; // 沒有可行 cell
}