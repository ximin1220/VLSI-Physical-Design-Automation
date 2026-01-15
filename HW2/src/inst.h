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
#include <list>
using namespace std;

/* =========================
 * 資料結構（全用 idx 關聯）
 * ========================= */
struct CELL {
    string name;        // 僅為輸出/除錯保留
    int    idx = -1;    // 在 cells 向量中的索引
    int    gain = 0;
    int    size = 0;
    bool   locked = false; 
    int   group  = 0;     // A=0, B=1 （初始分割會設定）
    vector<int> nets_arr;  // 連到的 net idx
    list<int>::iterator it;
};

struct NET {
    string name;        // 僅為輸出/除錯保留
    int    idx = -1;    // 在 nets 向量中的索引
    int    num_cell = 0;
    bool   is_cut  = false;
    int    A_num   = 0;
    int    B_num   = 0;
    bool critical = false;
    vector<int> cells_arr; // 連到的 cell idx
};

struct Instance {
    vector<CELL> cells;
    vector<NET>  nets;
    long long    total_size = 0; // cell size 總和
    long long A_size = 0;
    long long B_size = 0;
    int maxp = 0;
    long long cutsize = 0;
};
