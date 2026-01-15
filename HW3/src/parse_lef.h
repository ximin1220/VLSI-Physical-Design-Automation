#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
// #include "db.h"

void parseLEF(const std::string &lef_path, DesignDB &db) {
    std::ifstream fin(lef_path);
    if (!fin) {
        std::cerr << "Cannot open LEF: " << lef_path << "\n";
        exit(1);
    }

    std::string tok;
    std::string cur_macro_name;
    bool in_macro = false;

    while (fin >> tok) {
        
        if (tok == "UNITS") {
            while (fin >> tok) {
                if (tok == "DATABASE") {
                    std::string t_microns; 
                    int n;
                    fin >> t_microns >> n; 
                    db.lef_dbu_per_micron = n;
                } 
                else if (tok == "END") {
                    std::string section; fin >> section; 
                    if (section == "UNITS") break;
                }
            }
        }
        
        // ========= 【 T61 修正：增加 !in_macro 檢查 】 =========
        // 只有當 *不在* MACRO 內部時，"SITE" 才是 SITE 定義
        else if (tok == "SITE" && !in_macro) {
            Site site;
            fin >> site.name;
            std::string class_type;
            
            while (fin >> tok) {
                if (tok == "SIZE") {
                    double w, h;
                    std::string by;
                    fin >> w >> by >> h;
                    
                    if (db.lef_dbu_per_micron > 0) {
                        site.width_dbu  = (int)std::round(w * db.lef_dbu_per_micron);
                        site.height_dbu = (int)std::round(h * db.lef_dbu_per_micron);
                    }
                    if (site.name == "CoreSite") db.core_site_width_micron = w;
                }
                else if (tok == "CLASS") {
                    fin >> class_type; 
                }
                else if (tok == "END") {
                    std::string name_end;
                    fin >> name_end;
                    break; 
                }
            }
            
            if (site.name == "CoreSite" || class_type == "CORE") {
                db.core_site_width_dbu = site.width_dbu;
                if (db.core_site_width_micron == 0.0 && site.width_dbu > 0 && db.lef_dbu_per_micron > 0) {
                    db.core_site_width_micron = (double)site.width_dbu / db.lef_dbu_per_micron;
                }
            }
            db.sites[site.name] = site;
        }
        
        // ========= MACRO 區塊 =========
        else if (tok == "MACRO") {
            fin >> cur_macro_name;
            in_macro = true;
            db.macros[cur_macro_name].name = cur_macro_name;
        }
        else if (in_macro) {
            if (tok == "SIZE") {
                double w, h;
                std::string by;
                fin >> w >> by >> h;
                auto &m = db.macros[cur_macro_name];
                
                if (db.core_site_width_dbu > 0 && !m.is_block) {
                    if (db.core_site_width_micron > 0.0) {
                        int num_sites = (int)std::ceil(w / db.core_site_width_micron);
                        m.width_dbu = num_sites * db.core_site_width_dbu;
                    } else {
                        m.width_dbu = (int)std::round(w * db.lef_dbu_per_micron);
                    }
                } else {
                    m.width_dbu = (int)std::round(w * db.lef_dbu_per_micron);
                }
                m.height_dbu = (int)std::round(h * db.lef_dbu_per_micron);
            }
            else if (tok == "CLASS") {
                std::string cls;
                fin >> cls;
                if (cls == "BLOCK") {
                    db.macros[cur_macro_name].is_block = true;
                }
            }
            else if (tok == "END") {
                std::string name_end;
                fin >> name_end;
                if (name_end == cur_macro_name) {
                    in_macro = false;
                    cur_macro_name.clear();
                }
            }
        }
    }

    std::cerr << "[LEF] dbu_per_micron=" << db.lef_dbu_per_micron
              << " macros=" << db.macros.size()
              << " sites=" << db.sites.size()
              << " (core_site_width=" << db.core_site_width_dbu << " dbu)"
              << "\n";
}