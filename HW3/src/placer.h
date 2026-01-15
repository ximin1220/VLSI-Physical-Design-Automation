#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>
#include <vector>
#include <map>
#include "db.h"

void assignInstToRows(DesignDB &db);

class Placer {
private:
    DesignDB& db;
    std::unordered_map<int, std::set<int>> inst_to_nets_map;
    BinGrid grid;

    long long calculatePartialHPWL(const std::set<int> &affected_net_ids) {
        long long partial_hpwl = 0;
        for (int net_id : affected_net_ids) {
            const auto &net = db.nets[net_id];
            if (net.pins.empty()) continue;
            long long min_x = std::numeric_limits<long long>::max();
            long long max_x = std::numeric_limits<long long>::min();
            long long min_y = std::numeric_limits<long long>::max();
            long long max_y = std::numeric_limits<long long>::min();
            bool found = false;
            for (const auto &pin : net.pins) {
                long long px = 0, py = 0;
                bool ok = false;
                if (pin.is_port) {
                    auto it = db.io_pins.find(pin.port_name);
                    if (it != db.io_pins.end()) { px = it->second.x; py = it->second.y; ok = true; }
                } else if (pin.inst_id >= 0 && pin.inst_id < (int)db.instances.size()) {
                    const auto &inst = db.instances[pin.inst_id];
                    px = inst.x; py = inst.y; ok = true;
                }
                if (ok) {
                    min_x = std::min(min_x, px); max_x = std::max(max_x, px);
                    min_y = std::min(min_y, py); max_y = std::max(max_y, py);
                    found = true;
                }
            }
            if (found) partial_hpwl += (max_x - min_x) + (max_y - min_y);
        }
        return partial_hpwl;
    }

    int findNextLegalX(int current_x, int inst_width, const std::vector<std::pair<int, int>>& blockages) {
        if (blockages.empty()) return current_x;
        bool overlapping;
        do {
            overlapping = false;
            auto it_block = std::lower_bound(blockages.begin(), blockages.end(), std::make_pair(current_x, 0));
            if (it_block != blockages.begin()) {
                auto it_prev = std::prev(it_block);
                if (current_x < it_prev->second && (current_x + inst_width) > it_prev->first) {
                    current_x = it_prev->second;
                    overlapping = true;
                }
            }
            if (!overlapping && it_block != blockages.end()) {
                if (current_x < it_block->second && (current_x + inst_width) > it_block->first) {
                    current_x = it_block->second;
                    overlapping = true;
                }
            }
        } while (overlapping);
        return current_x;
    }

    void rebuildRowCellIds() {
        for (auto& row : db.rows) row.cell_ids.clear();
        int site_width = db.sites.count("CoreSite") ? db.sites.at("CoreSite").width_dbu : db.core_site_width_dbu;
        if (site_width <= 0) site_width = 200;
        for (int i = 0; i < (int)db.instances.size(); ++i) {
            auto& inst = db.instances[i];
            if (inst.is_fixed) continue;
            int best_row = -1;
            int best_dy = std::numeric_limits<int>::max();
            bool best_in_span = false;
            for (int r = 0; r < (int)db.rows.size(); ++r) {
                const auto& row = db.rows[r];
                int rh = 0;
                auto it = db.sites.find(row.site_name);
                if (it != db.sites.end()) rh = it->second.height_dbu;
                if (rh > 0 && inst.macro_height != rh) continue;
                int dy = std::abs(row.y - inst.y);
                long long span_left = row.x;
                long long span_right = row.x + (long long)row.site_count * site_width;
                bool in_span = (inst.x >= span_left) && (inst.x + inst.macro_width <= span_right);
                if (in_span) {
                    if (!best_in_span || dy < best_dy) { best_in_span = true; best_dy = dy; best_row = r; }
                } else if (!best_in_span && dy < best_dy) {
                    best_dy = dy; best_row = r;
                }
            }
            if (best_row < 0) continue;
            auto& row = db.rows[best_row];
            inst.row_id = best_row;
            inst.y = row.y;
            inst.orient = row.orient;
            long long span_left = row.x;
            long long span_right = row.x + (long long)row.site_count * site_width;
            long long clamped = inst.x;
            if (clamped < span_left) clamped = span_left;
            if (clamped + inst.macro_width > span_right) clamped = span_right - inst.macro_width;
            int aligned = (int)(span_left + ((clamped - span_left + site_width - 1) / site_width) * site_width);
            if (aligned + inst.macro_width > span_right) aligned = (int)(span_right - inst.macro_width);
            inst.x = aligned;
            row.cell_ids.push_back(i);
        }
        for (auto& row : db.rows) {
            std::sort(row.cell_ids.begin(), row.cell_ids.end(),
                      [&](int a, int b){ return db.instances[a].x < db.instances[b].x; });
        }
    }

    void rebuildBinGridFromDb() {
        clearBinGrid();
        populateBinGrid();
    }

public:
    Placer(DesignDB& database) : db(database) {
        for (int net_id = 0; net_id < (int)db.nets.size(); ++net_id) {
            const auto& net = db.nets[net_id];
            for (const auto& pin : net.pins) {
                if (!pin.is_port) inst_to_nets_map[pin.inst_id].insert(net_id);
            }
        }
    }

    void initializeBinGrid(int nx, int ny) {
        grid.init(db.die_x_max, db.die_y_max, nx, ny);
    }

    void clearBinGrid() {
        grid.clear();
    }

    void populateBinGrid() {
        for (int i = 0; i < (int)db.instances.size(); ++i) {
            if (!db.instances[i].is_fixed) grid.addInstance(db.instances[i], i);
        }
    }

    void runNbbSwap(int iterations) {
        rebuildRowCellIds();
        rebuildBinGridFromDb();

        std::vector<int> movable_inst_ids;
        for (int i = 0; i < (int)db.instances.size(); ++i) if (!db.instances[i].is_fixed) movable_inst_ids.push_back(i);
        if (movable_inst_ids.size() < 2) return;

        for (int i = 0; i < iterations; ++i) {
            int inst_id_A = movable_inst_ids[rand() % movable_inst_ids.size()];
            auto& instA = db.instances[inst_id_A];
            if (instA.row_id < 0) continue;

            std::set<int> nets_A;
            if (inst_to_nets_map.count(inst_id_A)) nets_A = inst_to_nets_map.at(inst_id_A);
            if (nets_A.empty()) continue;
            long long min_x=1e18, max_x=-1e18, min_y=1e18, max_y=-1e18;
            int pins_in_bbox = 0;
            for (int net_id : nets_A) {
                for (const auto& pin : db.nets[net_id].pins) {
                    if (pin.is_port) {
                        auto it = db.io_pins.find(pin.port_name);
                        if (it != db.io_pins.end()) {
                            min_x = std::min(min_x, (long long)it->second.x); max_x = std::max(max_x, (long long)it->second.x);
                            min_y = std::min(min_y, (long long)it->second.y); max_y = std::max(max_y, (long long)it->second.y);
                            pins_in_bbox++;
                        }
                    } else if (pin.inst_id != inst_id_A && pin.inst_id >= 0) {
                        const auto& inst = db.instances[pin.inst_id];
                        min_x = std::min(min_x, (long long)inst.x); max_x = std::max(max_x, (long long)inst.x);
                        min_y = std::min(min_y, (long long)inst.y); max_y = std::max(max_y, (long long)inst.y);
                        pins_in_bbox++;
                    }
                }
            }
            if (pins_in_bbox < 1) continue;

            int ideal_x = (int)((min_x + max_x) / 2);
            int ideal_y = (int)((min_y + max_y) / 2);

            auto bin_indices = grid.getBinIndex(ideal_x, ideal_y);
            if (grid.bins[bin_indices.first][bin_indices.second].cells_by_width.count(instA.macro_width) == 0) continue;
            auto& candidates = grid.bins[bin_indices.first][bin_indices.second].cells_by_width.at(instA.macro_width);
            if (candidates.empty()) continue;

            int inst_id_B = candidates[rand() % candidates.size()];
            auto& instB = db.instances[inst_id_B];
            if (inst_id_A == inst_id_B) continue;
            if (instB.row_id < 0 || instA.macro_height != instB.macro_height) continue;

            std::set<int> affected_net_ids;
            if (inst_to_nets_map.count(inst_id_A)) affected_net_ids.insert(inst_to_nets_map.at(inst_id_A).begin(), inst_to_nets_map.at(inst_id_A).end());
            if (inst_to_nets_map.count(inst_id_B)) affected_net_ids.insert(inst_to_nets_map.at(inst_id_B).begin(), inst_to_nets_map.at(inst_id_B).end());
            if (affected_net_ids.empty()) continue;

            long long hpwl_old = calculatePartialHPWL(affected_net_ids);

            std::string old_orient_A = instA.orient;
            std::string old_orient_B = instB.orient;
            std::swap(instA.x, instB.x);
            std::swap(instA.y, instB.y);
            std::swap(instA.row_id, instB.row_id);
            instA.orient = db.rows[instA.row_id].orient;
            instB.orient = db.rows[instB.row_id].orient;

            long long hpwl_new = calculatePartialHPWL(affected_net_ids);

            if (hpwl_new >= hpwl_old) {
                std::swap(instA.x, instB.x);
                std::swap(instA.y, instB.y);
                std::swap(instA.row_id, instB.row_id);
                instA.orient = old_orient_A;
                instB.orient = old_orient_B;
            }
        }
    }

    void runSlidingWindow(int window_size) {
        rebuildRowCellIds();

        int site_width = db.sites.count("CoreSite") ? db.sites.at("CoreSite").width_dbu : db.core_site_width_dbu;
        if (site_width == 0) site_width = 200;

        for (auto &row : db.rows) {
            if ((int)row.cell_ids.size() < 2) continue;
            const auto& blockages = row.blockages;
            for (int i = 0; i <= (int)row.cell_ids.size() - window_size; ++i) {
                std::vector<int> window_inst_ids;
                std::vector<int> window_indices;
                for (int j = 0; j < window_size; ++j) {
                    window_inst_ids.push_back(row.cell_ids[i+j]);
                    window_indices.push_back(j);
                }

                std::set<int> affected_net_ids;
                for (int inst_id : window_inst_ids) {
                    if (inst_to_nets_map.count(inst_id)) {
                        affected_net_ids.insert(inst_to_nets_map.at(inst_id).begin(), inst_to_nets_map.at(inst_id).end());
                    }
                }
                if (affected_net_ids.empty()) continue;

                long long hpwl_old = calculatePartialHPWL(affected_net_ids);
                long long best_hpwl = hpwl_old;
                std::vector<int> best_permutation = window_indices;
                std::vector<int> old_x;
                for (int id : window_inst_ids) old_x.push_back(db.instances[id].x);

                const int window_start_x = old_x[0];
                int window_end_x = (i + window_size < (int)row.cell_ids.size()) ?
                    db.instances[row.cell_ids[i + window_size]].x :
                    (row.x + (row.site_count * site_width));

                std::vector<int> current_permutation = window_indices;
                bool at_least_one_legal = false;

                do {
                    int current_x = window_start_x;
                    bool is_legal = true;

                    for (int k = 0; k < (int)window_inst_ids.size(); ++k) {
                        db.instances[window_inst_ids[k]].x = old_x[k];
                    }

                    for (int p_idx : current_permutation) {
                        int inst_id = window_inst_ids[p_idx];
                        int inst_width = db.instances[inst_id].macro_width;

                        auto it_block = std::lower_bound(blockages.begin(), blockages.end(), std::make_pair(current_x, 0));
                        if (it_block != blockages.end()) {
                            if (current_x < it_block->second && (current_x + inst_width) > it_block->first) current_x = it_block->second;
                        }
                        if (it_block != blockages.begin()) {
                            auto it_prev = std::prev(it_block);
                            if (current_x < it_prev->second && (current_x + inst_width) > it_prev->first) current_x = it_prev->second;
                        }

                        db.instances[inst_id].x = current_x;
                        if ((current_x + inst_width) > window_end_x) { is_legal = false; break; }
                        current_x += inst_width;
                    }
                    if (!is_legal) continue;
                    at_least_one_legal = true;

                    long long hpwl_new = calculatePartialHPWL(affected_net_ids);
                    if (hpwl_new < best_hpwl) {
                        best_hpwl = hpwl_new;
                        best_permutation = current_permutation;
                    }
                } while (std::next_permutation(current_permutation.begin(), current_permutation.end()));

                for (int k = 0; k < (int)window_inst_ids.size(); ++k) db.instances[window_inst_ids[k]].x = old_x[k];

                if (best_hpwl < hpwl_old && at_least_one_legal) {
                    int current_x = window_start_x;
                    for (int p_idx : best_permutation) {
                        int inst_id = window_inst_ids[p_idx];
                        int inst_width = db.instances[inst_id].macro_width;
                        auto it_block = std::lower_bound(blockages.begin(), blockages.end(), std::make_pair(current_x, 0));
                        if (it_block != blockages.end()) {
                            if (current_x < it_block->second && (current_x + inst_width) > it_block->first) current_x = it_block->second;
                        }
                        if (it_block != blockages.begin()) {
                            auto it_prev = std::prev(it_block);
                            if (current_x < it_prev->second && (current_x + inst_width) > it_prev->first) current_x = it_prev->second;
                        }
                        db.instances[inst_id].x = current_x;
                        current_x += inst_width;
                    }
                    std::sort(row.cell_ids.begin() + i, row.cell_ids.begin() + i + window_size,
                              [&](int a, int b){ return db.instances[a].x < db.instances[b].x; });
                }
            }
        }
    }

    void runGlobalInsertOrSwap() {
        rebuildRowCellIds();

        std::vector<std::pair<int,int>> movable_candidates;
        for (int i = 0; i < (int)db.instances.size(); ++i) {
            if (db.instances[i].is_fixed) continue;
            int deg = 0;
            auto it = inst_to_nets_map.find(i);
            if (it != inst_to_nets_map.end()) deg = (int)it->second.size();
            movable_candidates.push_back({deg, i});
        }
        if (movable_candidates.empty() || db.rows.empty()) return;
        std::sort(movable_candidates.begin(), movable_candidates.end(),
                  [](const std::pair<int,int>& a, const std::pair<int,int>& b){
                      if (a.first == b.first) return a.second < b.second;
                      return a.first > b.first;
                  });
        std::vector<int> movable_inst_ids;
        for (const auto& p : movable_candidates) movable_inst_ids.push_back(p.second);

        int site_width = db.sites.count("CoreSite") ? db.sites.at("CoreSite").width_dbu : (db.core_site_width_dbu > 0 ? db.core_site_width_dbu : 200);

        auto alignToSite = [&](int x, const Row& row) {
            int offset = (int)std::llround((double)(x - row.x) / site_width);
            return row.x + offset * site_width;
        };

        auto eraseFromRow = [&](Row& row, int inst_id) {
            for (auto it = row.cell_ids.begin(); it != row.cell_ids.end(); ++it) {
                if (*it == inst_id) { row.cell_ids.erase(it); break; }
            }
        };

        auto sortRow = [&](Row& row) {
            std::sort(row.cell_ids.begin(), row.cell_ids.end(), [&](int a, int b) {
                return db.instances[a].x < db.instances[b].x;
            });
        };

        int processed = 0;
        int max_cells = db.instances.size();
        for (int inst_id_A : movable_inst_ids) {
            if (processed++ >= max_cells) break;
            auto& instA = db.instances[inst_id_A];
            if (instA.row_id < 0) continue;
            if (inst_to_nets_map.count(inst_id_A) == 0) continue;

            const auto& nets_A = inst_to_nets_map.at(inst_id_A);
            if (nets_A.empty()) continue;

            std::vector<int> xs, ys;
            xs.reserve(nets_A.size() * 2);
            ys.reserve(nets_A.size() * 2);

            for (int net_id : nets_A) {
                if (net_id < 0 || net_id >= (int)db.nets.size()) continue;
                int x1 = std::numeric_limits<int>::max();
                int x2 = std::numeric_limits<int>::min();
                int y1 = std::numeric_limits<int>::max();
                int y2 = std::numeric_limits<int>::min();
                bool found = false;
                for (const auto& pin : db.nets[net_id].pins) {
                    int px = 0, py = 0;
                    bool ok = false;
                    if (pin.is_port) {
                        auto it = db.io_pins.find(pin.port_name);
                        if (it != db.io_pins.end()) { px = it->second.x; py = it->second.y; ok = true; }
                    } else if (pin.inst_id >= 0 && pin.inst_id < (int)db.instances.size() && pin.inst_id != inst_id_A) {
                        const auto& inst = db.instances[pin.inst_id];
                        px = inst.x; py = inst.y; ok = true;
                    }
                    if (ok) {
                        x1 = std::min(x1, px); x2 = std::max(x2, px);
                        y1 = std::min(y1, py); y2 = std::max(y2, py);
                        found = true;
                    }
                }
                if (found) {
                    xs.push_back(x1); xs.push_back(x2);
                    ys.push_back(y1); ys.push_back(y2);
                }
            }
            if (xs.size() < 2) continue;
            std::sort(xs.begin(), xs.end());
            std::sort(ys.begin(), ys.end());

            int optx1 = xs[xs.size()/2 - 1];
            int optx2 = xs[xs.size()/2];
            int opty1 = ys[ys.size()/2 - 1];
            int opty2 = ys[ys.size()/2];
            int opt_center_x = (optx1 + optx2) / 2;
            int opt_center_y = (opty1 + opty2) / 2;

            std::vector<std::pair<int,int>> row_candidates;
            for (int r = 0; r < (int)db.rows.size(); ++r) {
                if (db.rows[r].y < opty1 || db.rows[r].y > opty2) continue;
                row_candidates.push_back({std::abs(db.rows[r].y - opt_center_y), r});
            }
            if (row_candidates.empty()) {
                int row_height = db.sites.count(db.rows[0].site_name) ? db.sites.at(db.rows[0].site_name).height_dbu :
                                 (db.sites.count("CoreSite") ? db.sites.at("CoreSite").height_dbu : 2400);
                int nearest = (opt_center_y - db.rows[0].y) / row_height;
                if (nearest < 0) nearest = 0;
                if (nearest >= (int)db.rows.size()) nearest = (int)db.rows.size() - 1;
                row_candidates.push_back({0, nearest});
            }
            std::sort(row_candidates.begin(), row_candidates.end(),
                      [](const std::pair<int,int>& a, const std::pair<int,int>& b){
                          if (a.first == b.first) return a.second < b.second;
                          return a.first < b.first;
                      });
            if ((int)row_candidates.size() > 10) row_candidates.resize(10);

            long long base_hpwl = calculatePartialHPWL(nets_A);
            struct Move { long long delta=0; bool has=false; bool is_swap=false; int row=-1; int x=0; int swap_id=-1; };
            Move best;

            auto evalInsert = [&](int row_idx, int x_pos){
                auto& row = db.rows[row_idx];
                int old_x = instA.x, old_y = instA.y, old_row = instA.row_id;
                std::string old_orient = instA.orient;
                instA.x = x_pos; instA.y = row.y; instA.row_id = row_idx; instA.orient = row.orient;
                long long hpwl_new = calculatePartialHPWL(nets_A);
                long long delta = hpwl_new - base_hpwl;
                instA.x = old_x; instA.y = old_y; instA.row_id = old_row; instA.orient = old_orient;
                if (!best.has || delta < best.delta) { best.delta = delta; best.has = true; best.is_swap = false; best.row = row_idx; best.x = x_pos; best.swap_id = -1; }
            };

            auto evalSwap = [&](int inst_id_B){
                if (inst_id_B == inst_id_A) return;
                auto& instB = db.instances[inst_id_B];
                if (instB.is_fixed || instB.row_id < 0) return;
                if (instA.macro_width != instB.macro_width || instA.macro_height != instB.macro_height) return;
                std::set<int> affected = nets_A;
                if (inst_to_nets_map.count(inst_id_B)) affected.insert(inst_to_nets_map.at(inst_id_B).begin(), inst_to_nets_map.at(inst_id_B).end());
                if (affected.empty()) return;
                long long hpwl_old = calculatePartialHPWL(affected);
                std::string oa = instA.orient, ob = instB.orient;
                std::swap(instA.x, instB.x); std::swap(instA.y, instB.y); std::swap(instA.row_id, instB.row_id);
                instA.orient = db.rows[instA.row_id].orient; instB.orient = db.rows[instB.row_id].orient;
                long long hpwl_new = calculatePartialHPWL(affected);
                std::swap(instA.x, instB.x); std::swap(instA.y, instB.y); std::swap(instA.row_id, instB.row_id);
                instA.orient = oa; instB.orient = ob;
                long long delta = hpwl_new - hpwl_old;
                if (!best.has || delta < best.delta) { best.delta = delta; best.has = true; best.is_swap = true; best.row = -1; best.x = 0; best.swap_id = inst_id_B; }
            };

            for (auto rc : row_candidates) {
                int row_idx = rc.second;
                auto& row = db.rows[row_idx];
                if (db.sites.count(row.site_name) && db.sites.at(row.site_name).height_dbu != instA.macro_height) continue;

                std::vector<std::pair<int,int>> segments = row.blockages;
                segments.reserve(segments.size() + row.cell_ids.size());
                for (int cid : row.cell_ids) {
                    if (cid == inst_id_A) continue;
                    const auto& c = db.instances[cid];
                    segments.emplace_back(c.x, c.x + c.macro_width);
                }
                std::sort(segments.begin(), segments.end(),
                          [](const std::pair<int,int>& a, const std::pair<int,int>& b){
                              if (a.first == b.first) return a.second < b.second;
                              return a.first < b.first;
                          });
                std::vector<std::pair<int,int>> merged;
                for (auto s : segments) {
                    if (merged.empty() || s.first > merged.back().second) merged.push_back(s);
                    else merged.back().second = std::max(merged.back().second, s.second);
                }

                int row_left = row.x;
                int row_right = row.x + row.site_count * site_width;
                int cursor = row_left;
                auto considerGap = [&](int g0, int g1){
                    if (g1 - g0 < instA.macro_width) return;
                    int target = std::min(std::max(opt_center_x, g0), g1 - instA.macro_width);
                    int pos = alignToSite(target, row);
                    if (pos < g0) pos = g0;
                    if (pos + instA.macro_width > g1) pos = g1 - instA.macro_width;
                    evalInsert(row_idx, pos);
                };
                for (auto s : merged) { considerGap(cursor, s.first); cursor = std::max(cursor, s.second); }
                considerGap(cursor, row_right);

                if (!row.cell_ids.empty()) {
                    auto it = std::lower_bound(row.cell_ids.begin(), row.cell_ids.end(), opt_center_x,
                        [&](int id, int val){ return db.instances[id].x < val; });
                    if (it != row.cell_ids.end()) evalSwap(*it);
                    if (it != row.cell_ids.begin()) evalSwap(*std::prev(it));
                }
            }

            if (!best.has || best.delta >= 0) continue;

            if (!best.is_swap) {
                int old_row = instA.row_id;
                eraseFromRow(db.rows[old_row], inst_id_A);
                instA.x = best.x;
                instA.y = db.rows[best.row].y;
                instA.row_id = best.row;
                instA.orient = db.rows[best.row].orient;
                auto& dst = db.rows[best.row];
                auto it_ins = std::lower_bound(dst.cell_ids.begin(), dst.cell_ids.end(), instA.x,
                    [&](int id, int val){ return db.instances[id].x < val; });
                dst.cell_ids.insert(it_ins, inst_id_A);
                if (old_row != best.row) sortRow(db.rows[old_row]);
                sortRow(db.rows[best.row]);
            } else {
                int inst_id_B = best.swap_id;
                auto& instB = db.instances[inst_id_B];
                int rowA_old = instA.row_id, rowB_old = instB.row_id;
                eraseFromRow(db.rows[rowA_old], inst_id_A);
                eraseFromRow(db.rows[rowB_old], inst_id_B);
                std::swap(instA.x, instB.x); std::swap(instA.y, instB.y); std::swap(instA.row_id, instB.row_id);
                instA.orient = db.rows[instA.row_id].orient; instB.orient = db.rows[instB.row_id].orient;
                db.rows[instA.row_id].cell_ids.push_back(inst_id_A);
                db.rows[instB.row_id].cell_ids.push_back(inst_id_B);
                sortRow(db.rows[instA.row_id]);
                if (instA.row_id != instB.row_id) sortRow(db.rows[instB.row_id]);
            }
        }
    }

    void runRowNeighborhoodSwap(int max_cells, int row_window_half) {
        rebuildRowCellIds();

        std::vector<std::pair<int,int>> movable_candidates;
        for (int i = 0; i < (int)db.instances.size(); ++i) {
            if (!db.instances[i].is_fixed) {
                int deg = 0;
                auto it = inst_to_nets_map.find(i);
                if (it != inst_to_nets_map.end()) deg = (int)it->second.size();
                movable_candidates.push_back({deg, i});
            }
        }
        if (movable_candidates.empty()) return;
        std::sort(movable_candidates.begin(), movable_candidates.end(),
                  [](const std::pair<int,int>& a, const std::pair<int,int>& b){
                      if (a.first == b.first) return a.second < b.second;
                      return a.first > b.first;
                  });
        std::vector<int> order;
        for (auto& p : movable_candidates) order.push_back(p.second);

        int site_width = 200;
        if (db.sites.count("CoreSite")) site_width = db.sites.at("CoreSite").width_dbu;
        else if (db.core_site_width_dbu > 0) site_width = db.core_site_width_dbu;

        auto rowHeight = [&](int row_idx)->int{
            if (row_idx < 0 || row_idx >= (int)db.rows.size()) return 0;
            const auto& row = db.rows[row_idx];
            if (db.sites.count(row.site_name)) return db.sites.at(row.site_name).height_dbu;
            if (db.sites.count("CoreSite")) return db.sites.at("CoreSite").height_dbu;
            return 0;
        };

        auto alignToSite = [&](int x, const Row& row) {
            int offset = (int)std::llround((double)(x - row.x) / site_width);
            return row.x + offset * site_width;
        };

        auto eraseFromRow = [&](Row& row, int inst_id) {
            for (auto it = row.cell_ids.begin(); it != row.cell_ids.end(); ++it) {
                if (*it == inst_id) { row.cell_ids.erase(it); break; }
            }
        };

        auto sortRow = [&](Row& row) {
            std::sort(row.cell_ids.begin(), row.cell_ids.end(),
                      [&](int a, int b){ return db.instances[a].x < db.instances[b].x; });
        };

        int processed = 0;
        int success_moves = 0;

        for (int inst_id_A : order) {
            if (processed++ >= max_cells) break;
            auto& instA = db.instances[inst_id_A];
            if (instA.row_id < 0) continue;
            if (inst_to_nets_map.count(inst_id_A) == 0) continue;
            const auto& nets_A = inst_to_nets_map.at(inst_id_A);
            if (nets_A.empty()) continue;

            std::vector<int> xs, ys;
            xs.reserve(nets_A.size() * 2);
            ys.reserve(nets_A.size() * 2);
            for (int net_id : nets_A) {
                if (net_id < 0 || net_id >= (int)db.nets.size()) continue;
                int x1 = std::numeric_limits<int>::max();
                int x2 = std::numeric_limits<int>::min();
                int y1 = std::numeric_limits<int>::max();
                int y2 = std::numeric_limits<int>::min();
                bool found = false;
                for (const auto& pin : db.nets[net_id].pins) {
                    int px = 0, py = 0;
                    bool ok = false;
                    if (pin.is_port) {
                        auto itp = db.io_pins.find(pin.port_name);
                        if (itp != db.io_pins.end()) { px = itp->second.x; py = itp->second.y; ok = true; }
                    } else if (pin.inst_id >= 0 && pin.inst_id < (int)db.instances.size() && pin.inst_id != inst_id_A) {
                        const auto& inst = db.instances[pin.inst_id];
                        px = inst.x; py = inst.y; ok = true;
                    }
                    if (ok) {
                        x1 = std::min(x1, px); x2 = std::max(x2, px);
                        y1 = std::min(y1, py); y2 = std::max(y2, py);
                        found = true;
                    }
                }
                if (found) {
                    xs.push_back(x1); xs.push_back(x2);
                    ys.push_back(y1); ys.push_back(y2);
                }
            }
            if (xs.size() < 2) continue;
            std::sort(xs.begin(), xs.end());
            std::sort(ys.begin(), ys.end());
            int opt_center_x = (xs[xs.size()/2 - 1] + xs[xs.size()/2]) / 2;
            int opt_center_y = (ys[ys.size()/2 - 1] + ys[ys.size()/2]) / 2;

            int dir = (opt_center_y > instA.y) ? 1 : (opt_center_y < instA.y ? -1 : 0);
            if (dir == 0) continue;

            int rowA = instA.row_id;
            for (int step = 1; step <= row_window_half; ++step) {
                int row_idx = rowA + dir * step;
                if (row_idx < 0 || row_idx >= (int)db.rows.size()) break;
                int rh = rowHeight(row_idx);
                if (rh == 0 || rh != instA.macro_height) continue;
                auto& row = db.rows[row_idx];

                std::vector<std::pair<int,int>> segments = row.blockages;
                segments.reserve(segments.size() + row.cell_ids.size());
                for (int cid : row.cell_ids) {
                    if (cid == inst_id_A) continue;
                    const auto& c = db.instances[cid];
                    segments.emplace_back(c.x, c.x + c.macro_width);
                }
                std::sort(segments.begin(), segments.end(),
                          [](const std::pair<int,int>& a, const std::pair<int,int>& b){
                              if (a.first == b.first) return a.second < b.second;
                              return a.first < b.first;
                          });
                std::vector<std::pair<int,int>> merged;
                for (auto s : segments) {
                    if (merged.empty() || s.first > merged.back().second) merged.push_back(s);
                    else merged.back().second = std::max(merged.back().second, s.second);
                }

                int row_left = row.x;
                int row_right = row.x + row.site_count * site_width;
                int cursor = row_left;
                auto considerGap = [&](int g0, int g1, long long base_hpwl)->bool{
                    if (g1 - g0 < instA.macro_width) return false;
                    int target = std::min(std::max(opt_center_x, g0), g1 - instA.macro_width);
                    int pos = alignToSite(target, row);
                    if (pos < g0) pos = g0;
                    if (pos + instA.macro_width > g1) pos = g1 - instA.macro_width;

                    int old_x = instA.x, old_y = instA.y, old_row = instA.row_id;
                    std::string old_orient = instA.orient;
                    instA.x = pos; instA.y = row.y; instA.row_id = row_idx; instA.orient = row.orient;
                    long long hpwl_new = calculatePartialHPWL(nets_A);
                    if (hpwl_new < base_hpwl) {
                        eraseFromRow(db.rows[old_row], inst_id_A);
                        auto it_ins = std::lower_bound(row.cell_ids.begin(), row.cell_ids.end(), instA.x,
                            [&](int id, int val){ return db.instances[id].x < val; });
                        row.cell_ids.insert(it_ins, inst_id_A);
                        instA.row_id = row_idx;
                        success_moves++;
                        sortRow(db.rows[old_row]);
                        sortRow(db.rows[row_idx]);
                        return true;
                    }
                    instA.x = old_x; instA.y = old_y; instA.row_id = old_row; instA.orient = old_orient;
                    return false;
                };

                long long base_hpwl = calculatePartialHPWL(nets_A);
                bool moved = false;
                for (auto s : merged) { if (considerGap(cursor, s.first, base_hpwl)) { moved = true; break; } cursor = std::max(cursor, s.second); }
                if (!moved && considerGap(cursor, row_right, base_hpwl)) moved = true;

                if (moved) break;

                if (!row.cell_ids.empty()) {
                    auto it = std::lower_bound(row.cell_ids.begin(), row.cell_ids.end(), opt_center_x,
                        [&](int id, int val){ return db.instances[id].x < val; });
                    auto trySwap = [&](int inst_id_B){
                        if (inst_id_B == inst_id_A) return false;
                        auto& instB = db.instances[inst_id_B];
                        if (instB.is_fixed || instB.row_id < 0) return false;
                        if (instA.macro_width != instB.macro_width || instA.macro_height != instB.macro_height) return false;
                        std::set<int> affected = nets_A;
                        if (inst_to_nets_map.count(inst_id_B)) affected.insert(inst_to_nets_map.at(inst_id_B).begin(), inst_to_nets_map.at(inst_id_B).end());
                        if (affected.empty()) return false;
                        long long hpwl_old = calculatePartialHPWL(affected);
                        std::string oa = instA.orient, ob = instB.orient;
                        std::swap(instA.x, instB.x); std::swap(instA.y, instB.y); std::swap(instA.row_id, instB.row_id);
                        instA.orient = db.rows[instA.row_id].orient; instB.orient = db.rows[instB.row_id].orient;
                        long long hpwl_new = calculatePartialHPWL(affected);
                        if (hpwl_new < hpwl_old) {
                            eraseFromRow(db.rows[rowA], inst_id_A);
                            eraseFromRow(db.rows[instB.row_id], inst_id_B);
                            db.rows[instA.row_id].cell_ids.push_back(inst_id_A);
                            db.rows[instB.row_id].cell_ids.push_back(inst_id_B);
                            sortRow(db.rows[instA.row_id]);
                            if (instA.row_id != instB.row_id) sortRow(db.rows[instB.row_id]);
                            success_moves++;
                            return true;
                        }
                        std::swap(instA.x, instB.x); std::swap(instA.y, instB.y); std::swap(instA.row_id, instB.row_id);
                        instA.orient = oa; instB.orient = ob;
                        return false;
                    };
                    if (it != row.cell_ids.end() && trySwap(*it)) break;
                    if (it != row.cell_ids.begin() && trySwap(*std::prev(it))) break;
                }
            }
        }
    }

    void runLeftShiftGreedy() {
        rebuildRowCellIds();

        int site_width = db.sites.count("CoreSite") ? db.sites.at("CoreSite").width_dbu : db.core_site_width_dbu;
        if (site_width == 0) site_width = 200;

        for (auto& row : db.rows) {
            if (row.cell_ids.empty()) continue;
            int prev_end = row.x;
            for (int inst_id : row.cell_ids) {
                auto& inst = db.instances[inst_id];
                if (inst.is_fixed) { prev_end = inst.x + inst.macro_width; continue; }
                int aligned_prev = row.x + ((prev_end - row.x + site_width - 1) / site_width) * site_width;
                if (aligned_prev >= inst.x) {
                    prev_end = inst.x + inst.macro_width;
                    continue;
                }
                if (inst_to_nets_map.count(inst_id) == 0) {
                    inst.x = aligned_prev;
                    prev_end = inst.x + inst.macro_width;
                    continue;
                }
                const auto& nets = inst_to_nets_map.at(inst_id);
                long long hpwl_old = calculatePartialHPWL(nets);
                int old_x = inst.x;
                inst.x = aligned_prev;
                long long hpwl_new = calculatePartialHPWL(nets);
                if (hpwl_new >= hpwl_old) inst.x = old_x;
                prev_end = inst.x + inst.macro_width;
            }
            std::sort(row.cell_ids.begin(), row.cell_ids.end(), [&](int a, int b){
                return db.instances[a].x < db.instances[b].x;
            });
        }
    }

    void runRowLegalize() {
        rebuildRowCellIds();

        int site_width = db.sites.count("CoreSite") ? db.sites.at("CoreSite").width_dbu : db.core_site_width_dbu;
        if (site_width == 0) site_width = 200;

        auto alignToSite = [&](int x, const Row& row) {
            int offset = (int)std::llround((double)(x - row.x) / site_width);
            return row.x + offset * site_width;
        };

        for (int r = 0; r < (int)db.rows.size(); ++r) {
            auto& row = db.rows[r];
            if (row.cell_ids.empty()) continue;
            std::sort(row.cell_ids.begin(), row.cell_ids.end(),
                      [&](int a, int b){ return db.instances[a].x < db.instances[b].x; });

            int row_end = row.x + row.site_count * site_width;
            int cursor = row.x;
            for (int cid : row.cell_ids) {
                auto& inst = db.instances[cid];
                if (inst.is_fixed) {
                    cursor = std::max(cursor, inst.x + inst.macro_width);
                    continue;
                }
                int x_try = std::max(inst.x, cursor);
                x_try = alignToSite(x_try, row);
                x_try = findNextLegalX(x_try, inst.macro_width, row.blockages);
                if (x_try < cursor) x_try = cursor;
                if (x_try + inst.macro_width > row_end) x_try = row_end - inst.macro_width;
                if (x_try < row.x) x_try = row.x;
                inst.x = x_try;
                inst.y = row.y;
                inst.row_id = r;
                inst.orient = row.orient;
                cursor = inst.x + inst.macro_width;
            }
            std::sort(row.cell_ids.begin(), row.cell_ids.end(),
                      [&](int a, int b){ return db.instances[a].x < db.instances[b].x; });
        }
    }
};
