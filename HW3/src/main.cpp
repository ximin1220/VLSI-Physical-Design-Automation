#include <iostream>
#include <cmath>
#include <fstream>
#include <sstream>
#include <limits>
#include <cstdlib>
#include <ctime>
#include <chrono>
using namespace std;
#include "db.h"
#include "parse_lef.h"
#include "parse_def.h"
#include "placer.h"

using namespace std;

using Clock = std::chrono::high_resolution_clock;


long long calculateTotalHPWL(const DesignDB &db);
void linkInstMacro(DesignDB &db);
void assignInstToRows(DesignDB &db);
void writeDEF(const DesignDB &db, const std::string &def_in_path, const std::string &def_out_path);
void stampBlockages(DesignDB &db);

/*
./../bin/hw3 ../testcase/public1.lef ../testcase/public1.def ../output/output.def
*/
int main(int argc, char** argv) {


    auto t_total_start = Clock::now();
    srand(time(NULL));

    if (argc < 4) {
        std::cerr << "Usage: ./hw3 <input.lef> <input.def> <output.def>\n";
        return 1;
    }
    std::string lef_path = argv[1];
    std::string def_in   = argv[2];
    std::string def_out  = argv[3];


    DesignDB db;

    auto t_parse_start = Clock::now();

    parseLEF(lef_path, db);
    parseDEF(def_in, db);
    linkInstMacro(db);
    stampBlockages(db);
    assignInstToRows(db);

    auto t_parse_end = Clock::now();
    auto parse_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_parse_end - t_parse_start);


    long long initial_hpwl = calculateTotalHPWL(db);
    std::cout << "===== Initial HPWL Report =====" << std::endl;
    std::cout << "Calculated HPWL: " << initial_hpwl << std::endl;
    std::cout << "Expected HPWL:   " << 41714599410 << " (for public4)" << std::endl;
    std::cout << "=============================" << std::endl;


    auto t_algo_start = Clock::now();
    std::cout << "\n=== DEBUG: DB Integrity Check ===" << std::endl;
    std::cout << "DB Sites loaded: " << db.sites.size() << std::endl;
    for(auto& kv : db.sites) {
        std::cout << "  Key: '" << kv.first << "', W=" << kv.second.width_dbu << ", H=" << kv.second.height_dbu << std::endl;
    }
    if (!db.rows.empty()) {
        std::cout << "First Row: Name='" << db.rows[0].name << "', SiteName='" << db.rows[0].site_name << "'" << std::endl;
        if (db.sites.find(db.rows[0].site_name) == db.sites.end()) {
            std::cout << "  [ERROR] Row SiteName NOT FOUND in DB Sites!" << std::endl;
        } else {
            std::cout << "  [OK] Row SiteName found in DB." << std::endl;
        }
    } else {
        std::cout << "No Rows loaded!" << std::endl;
    }
    std::cout << "CoreSite Width (DBU): " << db.core_site_width_dbu << std::endl;
    std::cout << "=================================\n" << std::endl;


    Placer myPlacer(db);
    myPlacer.initializeBinGrid(100, 100);

    const long long max_algo_ms_budget = 260000;
    int target_outer_loops = 1;
    auto time_used_ms = [&](){
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - t_algo_start).count();
    };


    for (int iter = 0; iter < target_outer_loops && iter < 6; ++iter) {
        auto iter_start = Clock::now();
        std::cout << "--- Iteration " << iter << " ---" << std::endl;
        if (time_used_ms() > max_algo_ms_budget) {
            std::cout << "[Main] Time budget reached, stopping iterations.\n";
            break;
        }


        myPlacer.runGlobalInsertOrSwap();
        myPlacer.runSlidingWindow(2);
        cout << db.instances.size() << "\n";
        int rns_cells = std::min<int>(500000, db.instances.size());
        myPlacer.runRowNeighborhoodSwap(rns_cells, 3);
        myPlacer.runSlidingWindow(2);


        auto iter_end = Clock::now();
        if (iter == 0) {
            auto iter_ms = std::chrono::duration_cast<std::chrono::milliseconds>(iter_end - iter_start).count();
            if (iter_ms > 0) {
                int extra = (int)(280000.0 / iter_ms) - 1;
                if (extra < 0) extra = 0;
                target_outer_loops = 1 + extra;
            }
        }
    }



    myPlacer.runRowLegalize();


    auto t_algo_end = Clock::now();

    auto algo_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_algo_end - t_algo_start);


    long long final_hpwl = calculateTotalHPWL(db);
    std::cout << "===== Final HPWL Report =====" << std::endl;
    std::cout << "Initial HPWL: " << initial_hpwl << std::endl;
    std::cout << "Final HPWL:   " << final_hpwl << std::endl;
    std::cout << "Improvement:  " << (initial_hpwl - final_hpwl) << std::endl;
    std::cout << "Baseline:     " << 41714599410 << " (for public4)" << std::endl;
    std::cout << "=============================" << std::endl;



    std::cout << "[Main] Writing output DEF file to " << def_out << "..." << std::endl;
    writeDEF(db, def_in, def_out);
    std::cout << "[Main] ...Done." << std::endl;

    auto t_total_end = Clock::now();
    auto total_time_s = std::chrono::duration_cast<std::chrono::seconds>(t_total_end - t_total_start);
    auto algo_time_s_double = std::chrono::duration_cast<std::chrono::duration<double>>(t_algo_end - t_algo_start);

    std::cout << "\n===== Runtime Report (for HW3) =====" << std::endl;
    std::cout << "Parsing & Preprocessing: " << parse_time_ms.count() << " ms" << std::endl;
    std::cout << "Placement Algorithm:     " << algo_time_ms.count() << " ms (" << algo_time_s_double.count() << " s)" << std::endl;
    std::cout << "Total Execution Time:    " << total_time_s.count() << " s" << std::endl;
    std::cout << "====================================" << std::endl;

    return 0;
}


long long calculateTotalHPWL(const DesignDB &db) {
    long long total_hpwl = 0;
    for (const auto &net : db.nets) {
        if (net.pins.empty()) continue;
        long long min_x = std::numeric_limits<long long>::max();
        long long max_x = std::numeric_limits<long long>::min();
        long long min_y = std::numeric_limits<long long>::max();
        long long max_y = std::numeric_limits<long long>::min();
        bool at_least_one_pin_found = false;
        for (const auto &pin : net.pins) {
            long long pin_x = 0;
            long long pin_y = 0;
            bool pin_found = false;
            if (pin.is_port) {
                auto it = db.io_pins.find(pin.port_name);
                if (it != db.io_pins.end()) {
                    pin_x = it->second.x;
                    pin_y = it->second.y;
                    pin_found = true;
                }
            } else {
                if (pin.inst_id >= 0 && pin.inst_id < (int)db.instances.size()) {
                    const auto &inst = db.instances[pin.inst_id];
                    pin_x = inst.x;
                    pin_y = inst.y;
                    pin_found = true;
                }
            }
            if (pin_found) {
                min_x = std::min(min_x, pin_x);
                max_x = std::max(max_x, pin_x);
                min_y = std::min(min_y, pin_y);
                max_y = std::max(max_y, pin_y);
                at_least_one_pin_found = true;
            }
        }
        if (at_least_one_pin_found) {
            long long net_hpwl = (max_x - min_x) + (max_y - min_y);
            total_hpwl += net_hpwl;
        }
    }
    return total_hpwl;
}

void stampBlockages(DesignDB &db) {
    std::cout << "[Main] Stamping blockages..." << std::endl;

    for (const auto& inst : db.instances) {

        if (!inst.is_fixed) continue;


        int block_x_start = inst.x;
        int block_x_end = inst.x + inst.macro_width;
        int block_y_start = inst.y;
        int block_y_end = inst.y + inst.macro_height;


        for (auto& row : db.rows) {

            if (db.sites.find(row.site_name) == db.sites.end()) continue;
            int row_height = db.sites.at(row.site_name).height_dbu;

            int row_y_start = row.y;
            int row_y_end = row.y + row_height;



            bool y_overlaps = (block_y_start < row_y_end) && (block_y_end > row_y_start);

            if (y_overlaps) {

                row.blockages.push_back( {block_x_start, block_x_end} );
            }
        }
    }


    for (auto& row : db.rows) {
        std::sort(row.blockages.begin(), row.blockages.end());
    }
}


void linkInstMacro(DesignDB &db) {
    for (auto &inst : db.instances) {
        auto it = db.macros.find(inst.macro_name);
        if (it == db.macros.end()) continue;
        inst.macro_width  = it->second.width_dbu;
        inst.macro_height = it->second.height_dbu;
    }
}



void assignInstToRows(DesignDB &db) {
    std::unordered_map<int,int> y2row;
    for (int i = 0; i < (int)db.rows.size(); ++i) {
        y2row[db.rows[i].y] = i;
    }
    for (int i = 0; i < (int)db.instances.size(); ++i) {
        auto &inst = db.instances[i];
        if (inst.is_fixed) continue;
        auto it = y2row.find(inst.y);
        if (it == y2row.end()) {
            continue;
        }
        int row_id = it->second;
        inst.row_id = row_id;
        db.rows[row_id].cell_ids.push_back(i);
    }
    for (auto &row : db.rows) {
        std::sort(row.cell_ids.begin(), row.cell_ids.end(),
                  [&](int a, int b){
                      return db.instances[a].x < db.instances[b].x;
                  });
    }
}




void writeDEF(const DesignDB &db, const std::string &def_in_path, const std::string &def_out_path) {
    std::ifstream fin(def_in_path);
    if (!fin) {
        std::cerr << "Error: Cannot open input DEF for writing: " << def_in_path << std::endl;
        return;
    }
    std::ofstream fout(def_out_path);
    if (!fout) {
        std::cerr << "Error: Cannot open output DEF for writing: " << def_out_path << std::endl;
        return;
    }

    std::string line;
    bool in_components = false;
    std::string pending_instName;

    while (std::getline(fin, line)) {

        std::stringstream ss(line);
        std::string tok;
        if (!(ss >> tok)) {
            fout << line << "\n";
            continue;
        }


        if (tok == "COMPONENTS") {
            in_components = true;
            fout << line << "\n";
            continue;
        } else if (tok == "END") {
            std::string tok2;
            ss >> tok2;
            if (tok2 == "COMPONENTS") {
                in_components = false;
                pending_instName.clear();
            }
            fout << line << "\n";
            continue;
        }


        if (!in_components) {
            fout << line << "\n";
            continue;
        }



        if (tok == "-") {
            std::string instName, macroName;
            ss >> instName >> macroName;
            pending_instName = instName;

            std::string rest_of_line;
            std::getline(ss, rest_of_line);

            if (rest_of_line.find("+ PLACED") != std::string::npos) {


                pending_instName.clear();

                auto it = db.inst_name_to_id.find(instName);
                if (it == db.inst_name_to_id.end()) {
                    fout << line << "\n";
                } else {
                    const auto& inst = db.instances[it->second];
                    fout << "- " << instName << " " << macroName
                         << " + PLACED ( " << inst.x << " " << inst.y
                         << " ) " << inst.orient << " ;" << "\n";
                }
            }
            else {




                fout << line << "\n";


            }
        }
        else if ( (tok == "+" || starts_with(tok, "+")) && !pending_instName.empty() ) {


            std::string status;
            if (tok == "+") ss >> status;
            else status = tok.substr(1);

            if (status == "PLACED") {

                auto it = db.inst_name_to_id.find(pending_instName);
                if (it == db.inst_name_to_id.end()) {
                    fout << line << "\n";
                } else {
                    const auto& inst = db.instances[it->second];



                    fout << "  + PLACED ( " << inst.x << " " << inst.y
                         << " ) " << inst.orient;


                    if (line.find(';') != std::string::npos) {
                        fout << " ;";
                        pending_instName.clear();
                    }
                    fout << "\n";
                }
            } else {


                fout << line << "\n";
            }
        }
        else if (in_components && tok == ";" && !pending_instName.empty()) {



            fout << line << "\n";
            pending_instName.clear();
        }
        else {







            if (in_components && tok == ";" && pending_instName.empty()) {


            } else {
                fout << line << "\n";
            }
        }
    }
}
