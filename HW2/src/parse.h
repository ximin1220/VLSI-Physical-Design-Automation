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
// #include "inst.h"

/** 工具：吃掉 // 後面的註解 */


using namespace std;


/* ================
 * 小工具
 * ================ */
static inline string stripComment(const string& s) {
    size_t pos = s.find("//");
    return (pos == string::npos) ? s : s.substr(0, pos);
}

/** 讀 .txt：只用到
 *  NumCells N
 *  Cell <cellName> <size>
 *  ...
 *  NumNets M
 *  Net <netName> <k>
 *  Cell <cellName>
 *  (重複 k 次)
 */
/* =========================
 * 解析器：將名字轉成 idx，最後只存 idx
 * 支援格式：
 *   NumCells N
 *   Cell <cellName> <size>
 *   ...
 *   NumNets M
 *   Net <netName> <k>
 *   Cell <cellName>
 *   (重複 k 次)
 * ========================= */
bool parseInput(const string& path, Instance& inst) {
    ifstream fin(path);
    if (!fin) {
        cerr << "Cannot open " << path << "\n";
        return false;
    }

    string line;
    int expectedCells = -1, expectedNets = -1;

    // 僅在 parse 階段用來把名字轉成 idx
    unordered_map<string,int> cellIndexByName;

    while (getline(fin, line)) {
        line = stripComment(line);
        if (line.empty()) continue;
        stringstream ss(line);
        string tok; ss >> tok;
        if (tok.empty()) continue;

        if (tok == "NumCells") {
            ss >> expectedCells;
            inst.cells.reserve(expectedCells);

        } else if (tok == "Cell") {
            // 可能是定義 cell（有 size），也可能是 Net 區塊中的 "Cell <name>"（無 size）
            string cname; ss >> cname;
            int csize;
            if (ss >> csize) {
                CELL c;
                c.name = cname;
                c.size = csize;
                c.idx  = (int)inst.cells.size();
                cellIndexByName[c.name] = c.idx;
                inst.total_size += c.size;
                inst.cells.push_back(std::move(c));
            } else {
                // Net 區塊裡的 "Cell <name>"，由 Net 區塊處理
            }

        } else if (tok == "NumNets") {
            ss >> expectedNets;
            inst.nets.reserve(expectedNets);

        } else if (tok == "Net") {
            string nname; int k; ss >> nname >> k;
            NET n;
            n.name = nname;
            n.idx  = (int)inst.nets.size();
            n.num_cell = k;

            // 讀 k 行："Cell <cellName>"
            vector<int> cell_idx_list; 
            cell_idx_list.reserve(k);

            for (int i = 0; i < k; ++i) {
                string l2;
                if (!getline(fin, l2)) {
                    cerr << "Unexpected EOF in Net block\n";
                    return false;
                }
                l2 = stripComment(l2);
                if (l2.empty()) { --i; continue; }

                string ctok, cname;
                stringstream ss2(l2);
                ss2 >> ctok >> cname;
                if (ctok != "Cell" || cname.empty()) {
                    cerr << "Bad net cell line: " << l2 << "\n";
                    return false;
                }

                auto it = cellIndexByName.find(cname);
                if (it == cellIndexByName.end()) {
                    cerr << "Cell " << cname << " used in net " << nname << " but not defined.\n";
                    return false;
                }
                cell_idx_list.push_back(it->second);
            }

            // 存 net 的相鄰表（cell idx）
            n.cells_arr = std::move(cell_idx_list);

            // push 進 nets
            inst.nets.push_back(std::move(n));

            // 反向掛回 cell 的相鄰表（net idx）
            int this_net_idx = inst.nets.back().idx;
            for (int cidx : inst.nets.back().cells_arr) {
                inst.cells[cidx].nets_arr.push_back(this_net_idx);
            }

        } else {
            // 其他 token 略過
        }
    }

    for(auto& cell : inst.cells) {
        inst.maxp = max(inst.maxp, (int)cell.nets_arr.size());
    }

    // 粗檢提示（不強制失敗）
    if (expectedCells >= 0 && (int)inst.cells.size() != expectedCells) {
        cerr << "NumCells mismatch. Declared " << expectedCells
             << ", got " << inst.cells.size() << "\n";
    }
    if (expectedNets >= 0 && (int)inst.nets.size() != expectedNets) {
        cerr << "NumNets mismatch. Declared " << expectedNets
             << ", got " << inst.nets.size() << "\n";
    }
    return true;
}