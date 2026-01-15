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
using namespace std;
// 依目前的分組重新計算 cut size（可選）
static long long recomputeCutSize(const Instance& inst) {
    long long cut = 0;
    for (const auto& net : inst.nets) {
        if (net.cells_arr.empty()) continue;
        // 看這個 net 是否跨越多個 group（一般化到 k-way 也可用）
        int g0 = inst.cells[net.cells_arr[0]].group;
        bool multi = false;
        for (size_t i = 1; i < net.cells_arr.size(); ++i) {
            int cid = net.cells_arr[i];
            if (inst.cells[cid].group != g0) { multi = true; break; }
        }
        if (multi) ++cut;
    }
    return cut;
}

// 輸出 .out 檔（格式：CutSize / GroupA / GroupB）
static bool writeOutput(const Instance& inst, const string& out_path,
                        bool sort_names = false, bool recompute_cut = false) {
    ofstream fout(out_path);
    if (!fout) {
        cerr << "Cannot open output file: " << out_path << "\n";
        return false;
    }

    long long cut = recompute_cut ? recomputeCutSize(inst) : inst.cutsize;

    // 分組收集
    vector<string> A_names, B_names;
    A_names.reserve(inst.cells.size());
    B_names.reserve(inst.cells.size());
    for (const auto& c : inst.cells) {
        if (c.group == 0) A_names.push_back(c.name);
        else              B_names.push_back(c.name);
    }

    if (sort_names) {
        sort(A_names.begin(), A_names.end());
        sort(B_names.begin(), B_names.end());
    }

    // 照題目範例格式輸出
    fout << "CutSize " << cut << "\n";
    fout << "GroupA " << A_names.size() << "\n";
    for (const auto& nm : A_names) fout << nm << "\n";
    fout << "\n"; // 依範例在兩組之間加一空行（想拿掉可移除）

    fout << "GroupB " << B_names.size() << "\n";
    for (const auto& nm : B_names) fout << nm << "\n";

    fout.flush();
    return true;
}

// 4-way 版本（保留你原本 writeOutput，這個取名不同）
void writeOutput4way(const Instance& inst, const string& path) {
    ofstream ofs(path);
    if (!ofs) { cerr << "Cannot open " << path << "\n"; return; }
    // ofs << "CutSize " << inst.cutsize << "\n";
    auto kway_cut = [&](){
        int cut = 0;
        for (auto &net : inst.nets) {
            bool seen[4] = {false,false,false,false};
            int kinds = 0;
            for (int u : net.cells_arr) {
                int g = inst.cells[u].group;
                if (g>=0 && g<4 && !seen[g]) { seen[g]=true; kinds++; }
                if (kinds >= 2) { cut++; break; }
            }
        }
        return cut;
    };
    ofs << "CutSize " << kway_cut() << "\n";
    vector<int> G[4];
    for (auto &c : inst.cells) {
        int g = c.group;
        if (g < 0 || g > 3) g = 0; // 保守處理
        G[g].push_back(c.idx);
    }

    auto print_group = [&](int gi, const char* label){
        ofs << label << " " << G[gi].size() << "\n";
        for (int idx : G[gi]) ofs << inst.cells[idx].name << "\n";
        ofs << "\n";
    };

    print_group(0, "GroupA");
    print_group(1, "GroupB");
    print_group(2, "GroupC");
    print_group(3, "GroupD");

    // 如果你要順便寫 cutsize（4-way 定義：一條 net 連到 ≥2 個不同 group 就算 cut）
    // 可以在這裡加上：
    
    ofs.close();
}

