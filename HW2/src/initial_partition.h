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
#include <numeric>
// #include "inst.h"
using namespace std;

/** Greedy-by-size 初始 2-way 分割
 *  - 依 cell size 由大到小放入目前總 size 較小的一側
 *  - 避免超過 upper 上限；最後若低於 lower，下做修正搬移
 *  回傳 pair<sumA,sumB>，同時會把 inst.cells[i].group 設為 0/1
 */
/* =============================================
 * Greedy-by-size 初始 2-way 分割（含 balance 修正）
 * - 依 cell size 由大到小放入目前 sum 較小的一側
 * - 避免超過 upper；若最後低於 lower，從另一側搬最小元素到達標
 * 備註：
 *   lower/upper 是比例（例如 0.45, 0.55）
 * 回傳：<sumA, sumB>
 * ============================================= */

pair<long long, long long>
bfs_initial_partition_2way(Instance &inst, double lower = 0.45, double upper = 0.55)
{
    const long long TOT = inst.total_size;
    if (TOT == 0)
        return {0, 0};
    const long long loCap = (long long)ceil(lower * TOT);
    const long long hiCap = (long long)ceil(upper * TOT);

    const int n = (int)inst.cells.size();
    vector<char> assigned(n, 0); // 只在真正放入某組時才標記

    long long sumA = 0, sumB = 0;

    // 依序從尚未分配的 seed 出發做 BFS，把強連結的先裝進較空的一側
    for (int seed = 0; seed < n; ++seed)
    {
        if (assigned[seed])
            continue;

        // 選擇這個叢集要優先放 A 還是 B（盡量塞到較小且未達上限的一側）
        bool toA = ((sumA < sumB && sumA < hiCap) || (sumB >= hiCap));
        long long &groupSum = toA ? sumA : sumB;
        bool groupFlag = toA ? 0 : 1;

        queue<int> q;
        q.push(seed);

        // 注意：這裡不用 visited；只要沒有 assigned，就允許被不同叢集再次嘗試
        while (!q.empty())
        {
            int u = q.front();
            q.pop();
            if (assigned[u])
                continue; // 已被其他叢集先放走

            int w = inst.cells[u].size;
            if (groupSum + w <= hiCap)
            {
                // 可以放入本組
                inst.cells[u].group = groupFlag;
                assigned[u] = 1;
                groupSum += w;

                // 擴張鄰居
                for (int nid : inst.cells[u].nets_arr)
                {
                    for (int v : inst.nets[nid].cells_arr)
                    {
                        if (!assigned[v])
                            q.push(v);
                    }
                }
            }
            // 否則：放不下就跳過 u（不標記 assigned），留待之後的叢集或另一組處理
        }

        // 若兩邊下限都已達成，就可以提前停止叢集擴張
        if (sumA >= loCap && sumB >= loCap)
            break;
    }

    // 把尚未分配的節點補進能容納的一側（優先放較小的一側）
    for (int i = 0; i < n; ++i)
    {
        if (assigned[i])
            continue;
        int w = inst.cells[i].size;
        if ((sumA <= sumB && sumA + w <= hiCap) || (sumB + w > hiCap))
        {
            inst.cells[i].group = 0;
            assigned[i] = 1;
            sumA += w;
        }
        else if (sumB + w <= hiCap)
        {
            inst.cells[i].group = 1;
            assigned[i] = 1;
            sumB += w;
        }
        else
        {
            // 兩側都超上限會發生在極端 case（例如某單顆很大）
            // 仍需放入較小的一側，後續再由 balance 修正
            if (sumA <= sumB)
            {
                inst.cells[i].group = 0;
                assigned[i] = 1;
                sumA += w;
            }
            else
            {
                inst.cells[i].group = 1;
                assigned[i] = 1;
                sumB += w;
            }
        }
    }

    // --- balance 修正：若一側低於下限，從另一側搬最小的過來（不破壞對方下限與我方上限） ---
    auto collect = [&](int g)
    {
        vector<int> v;
        for (int i = 0; i < n; ++i)
            if (inst.cells[i].group == g)
                v.push_back(i);
        sort(v.begin(), v.end(), [&](int a, int b)
             {
            if (inst.cells[a].size != inst.cells[b].size)
                return inst.cells[a].size < inst.cells[b].size;
            return inst.cells[a].name < inst.cells[b].name; });
        return v;
    };

    if (sumA < loCap)
    {
        auto B = collect(1);
        for (int cid : B)
        {
            int w = inst.cells[cid].size;
            if (sumA + w <= hiCap && sumB - w >= loCap)
            {
                inst.cells[cid].group = 0;
                sumA += w;
                sumB -= w;
                if (sumA >= loCap)
                    break;
            }
        }
    }
    else if (sumB < loCap)
    {
        auto A = collect(0);
        for (int cid : A)
        {
            int w = inst.cells[cid].size;
            if (sumB + w <= hiCap && sumA - w >= loCap)
            {
                inst.cells[cid].group = 1;
                sumB += w;
                sumA -= w;
                if (sumB >= loCap)
                    break;
            }
        }
    }

    return {sumA, sumB};
}

// 回傳 pair(sumA, sumB)
pair<long long, long long>
new_initial_partition_2way(Instance &inst, double lower_ratio = 0.45, double upper_ratio = 0.55)
{
    const long long TOT = inst.total_size;
    if (TOT == 0)
        return {0, 0};

    const long long loCap = (long long)ceil(lower_ratio * TOT);
    const long long hiCap = (long long)ceil(upper_ratio * TOT);

    const int n = (int)inst.cells.size();
    const int m = (int)inst.nets.size();

    // 先清空群組計數（A_num/B_num）
    for (int nid = 0; nid < m; ++nid)
    {
        inst.nets[nid].A_num = 0;
        inst.nets[nid].B_num = 0;
    }

    vector<char> assigned(n, 0);
    long long sumA = 0, sumB = 0;

    auto place = [&](int u, int g)
    {
        inst.cells[u].group = g;
        assigned[u] = 1;
        if (g == 0)
            sumA += inst.cells[u].size;
        else
            sumB += inst.cells[u].size;
        for (int nid : inst.cells[u].nets_arr)
        {
            if (g == 0)
                ++inst.nets[nid].A_num;
            else
                ++inst.nets[nid].B_num;
        }
    };

    auto deg_of_net = [&](int nid)
    { return (int)inst.nets[nid].cells_arr.size(); };

    // 向某側的傾向分數：網內已在該側的比例相加
    auto score_side = [&](int u, int side) -> double
    {
        double s = 0.0;
        for (int nid : inst.cells[u].nets_arr)
        {
            int d = deg_of_net(nid);
            if (d <= 0)
                continue;
            int inA = inst.nets[nid].A_num;
            int inB = inst.nets[nid].B_num;
            s += (side == 0 ? (double)inA / d : (double)inB / d);
        }
        return s;
    };

    // cohesion：偏好小網、自己 size 小
    auto cohesion = [&](int u) -> double
    {
        double s = 0.0;
        for (int nid : inst.cells[u].nets_arr)
        {
            int d = deg_of_net(nid);
            if (d > 1)
                s += 1.0 / (d - 1);
        }
        return s / max(1, inst.cells[u].size);
    };

    // seed 順序（高 cohesion 先）
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(),
         [&](int a, int b)
         { return cohesion(a) > cohesion(b); });

    // frontier 佇列
    queue<int> qA, qB;

    // 大網節流
    const int NET_CAP = 64; // 你可視資料調整
    const int K_TAKE = 8;   // 大網只推前 K 個候選

    auto push_neighbors = [&](int u)
    {
        for (int nid : inst.cells[u].nets_arr)
        {
            auto &vec = inst.nets[nid].cells_arr;
            if ((int)vec.size() > NET_CAP)
            {
                // 選尚未 assigned 的前 K_TAKE 個 size 較小者
                vector<int> cand;
                cand.reserve(vec.size());
                for (int v : vec)
                    if (!assigned[v])
                        cand.push_back(v);
                if (!cand.empty())
                {
                    if ((int)cand.size() > K_TAKE)
                    {
                        nth_element(cand.begin(), cand.begin() + K_TAKE, cand.end(),
                                    [&](int a, int b)
                                    { return inst.cells[a].size < inst.cells[b].size; });
                        cand.resize(K_TAKE);
                    }
                    for (int v : cand)
                    {
                        double a = score_side(v, 0), b = score_side(v, 1);
                        if (a >= b)
                            qA.push(v);
                        else
                            qB.push(v);
                    }
                }
            }
            else
            {
                for (int v : vec)
                    if (!assigned[v])
                    {
                        double a = score_side(v, 0), b = score_side(v, 1);
                        if (a >= b)
                            qA.push(v);
                        else
                            qB.push(v);
                    }
            }
        }
    };

    // 逐 seed 啟動叢集擴張（雙前沿）
    for (int s : order)
    {
        if (assigned[s])
            continue;

        // 選擇這個叢集優先塞哪一側（容量導向）
        bool toA = ((sumA < sumB && sumA < hiCap) || (sumB >= hiCap));
        long long &groupSum = toA ? sumA : sumB;

        // 先放下 seed
        int w = inst.cells[s].size;
        if (groupSum + w <= hiCap)
        {
            place(s, toA ? 0 : 1);
            push_neighbors(s);
        }
        else
        {
            // 當前主側放不下，試另一側；都不行就先跳過，等收尾
            bool altOK = (!toA) ? (sumA + w <= hiCap) : (sumB + w <= hiCap);
            if (altOK)
            {
                place(s, toA ? 1 : 0);
                push_neighbors(s);
            }
        }

        // 交替從 qA / qB 取點擴張
        while (!qA.empty() || !qB.empty())
        {
            // 容量都已達下限可提前停
            if (sumA >= loCap && sumB >= loCap)
                break;

            // 取 A
            if (!qA.empty())
            {
                int u = qA.front();
                qA.pop();
                if (!assigned[u])
                {
                    int wu = inst.cells[u].size;
                    double a = score_side(u, 0), b = score_side(u, 1);
                    bool preferA = (a >= b);
                    if (preferA && sumA + wu <= hiCap)
                    {
                        place(u, 0);
                        push_neighbors(u);
                    }
                    else if (sumB + wu <= hiCap)
                    {
                        place(u, 1);
                        push_neighbors(u);
                    }
                }
            }
            // 取 B
            if (!qB.empty())
            {
                int u = qB.front();
                qB.pop();
                if (!assigned[u])
                {
                    int wu = inst.cells[u].size;
                    double a = score_side(u, 0), b = score_side(u, 1);
                    bool preferB = (b > a);
                    if (preferB && sumB + wu <= hiCap)
                    {
                        place(u, 1);
                        push_neighbors(u);
                    }
                    else if (sumA + wu <= hiCap)
                    {
                        place(u, 0);
                        push_neighbors(u);
                    }
                }
            }
        }

        if (sumA >= loCap && sumB >= loCap)
            break;
    }

    // 收尾：把未分配者補進（優先連線偏好，其次容量、最後較小側）
    for (int u = 0; u < n; ++u)
        if (!assigned[u])
        {
            int w = inst.cells[u].size;
            double a = score_side(u, 0), b = score_side(u, 1);

            auto try_put = [&](int side) -> bool
            {
                if (side == 0)
                {
                    if (sumA + w <= hiCap)
                    {
                        place(u, 0);
                        return true;
                    }
                }
                else
                {
                    if (sumB + w <= hiCap)
                    {
                        place(u, 1);
                        return true;
                    }
                }
                return false;
            };

            bool done = false;
            if (a > b)
                done = try_put(0) || try_put(1);
            else
                done = try_put(1) || try_put(0);

            if (!done)
            {
                // 雙方都卡容量，硬塞較小側（等後續 FM 修）
                if (sumA <= sumB)
                    place(u, 0);
                else
                    place(u, 1);
            }
        }

    // Balance 修正：若一側低於下限，就從另一側搬「跨邊多、同邊少」且 size 小的
    auto ext_minus_int = [&](int u, int g) -> int
    {
        int ext = 0, intl = 0;
        for (int nid : inst.cells[u].nets_arr)
        {
            int a = inst.nets[nid].A_num, b = inst.nets[nid].B_num;
            if (g == 0)
            {
                intl += (a > 0);
                ext += (b > 0);
            }
            else
            {
                intl += (b > 0);
                ext += (a > 0);
            }
        }
        return ext - intl;
    };

    auto collect_sorted = [&](int g)
    {
        vector<int> v;
        v.reserve(n);
        for (int i = 0; i < n; ++i)
            if (inst.cells[i].group == g)
                v.push_back(i);
        sort(v.begin(), v.end(), [&](int a, int b)
             {
            // 越可能降 cut 的越前（ext-int 大），同分比 size 小
            auto ka = make_pair(ext_minus_int(a, g), -inst.cells[a].size);
            auto kb = make_pair(ext_minus_int(b, g), -inst.cells[b].size);
            return ka > kb; });
        return v;
    };

    if (sumA < loCap)
    {
        auto B = collect_sorted(1);
        for (int u : B)
        {
            int w = inst.cells[u].size;
            if (sumA + w <= hiCap && sumB - w >= loCap)
            {
                // 從 B 移到 A
                inst.cells[u].group = 0;
                sumA += w;
                sumB -= w;
                for (int nid : inst.cells[u].nets_arr)
                {
                    --inst.nets[nid].B_num;
                    ++inst.nets[nid].A_num;
                }
                if (sumA >= loCap)
                    break;
            }
        }
    }
    else if (sumB < loCap)
    {
        auto A = collect_sorted(0);
        for (int u : A)
        {
            int w = inst.cells[u].size;
            if (sumB + w <= hiCap && sumA - w >= loCap)
            {
                // 從 A 移到 B
                inst.cells[u].group = 1;
                sumB += w;
                sumA -= w;
                for (int nid : inst.cells[u].nets_arr)
                {
                    --inst.nets[nid].A_num;
                    ++inst.nets[nid].B_num;
                }
                if (sumB >= loCap)
                    break;
            }
        }
    }

    return {sumA, sumB};
}
