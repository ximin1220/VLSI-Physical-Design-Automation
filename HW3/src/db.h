#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cmath>

struct Macro {
    std::string name;
    int width_dbu = 0;   
    int height_dbu = 0;  
    bool is_block = false; 
};

struct Site {
    std::string name;
    int width_dbu = 0;
    int height_dbu = 0;
};

struct Row {
    std::string name;
    std::string site_name;
    int x = 0, y = 0;        
    int site_count = 0;      
    int step_x = 0;          
    std::string orient;      
    std::vector<int> cell_ids; 
    std::vector<std::pair<int, int>> blockages;
};

struct Inst {
    std::string name;        
    std::string macro_name;  
    int x = 0, y = 0;        
    std::string orient;      
    bool is_fixed = false;   
    int macro_width = 0;     
    int macro_height = 0;
    int row_id = -1;         
};

struct NetPin {
    bool is_port = false;    
    int inst_id = -1;        
    std::string pin_name;    
    std::string port_name;   
};

struct Net {
    std::string name;
    std::vector<NetPin> pins;
};

struct IOPin {
    std::string name;
    int x = 0;
    int y = 0;
    std::string orient;  
};


struct DesignDB {
    int lef_dbu_per_micron = 0;
    int def_dbu_per_micron = 0;

    std::unordered_map<std::string, Site>  sites;
    std::unordered_map<std::string, Macro> macros;

    std::vector<Row> rows;
    std::vector<Inst> instances;
    std::unordered_map<std::string, int> inst_name_to_id;

    std::vector<Net> nets;
    std::unordered_map<std::string, int> net_name_to_id;

    std::unordered_map<std::string, IOPin> io_pins;

    int core_site_width_dbu = 0; 
    double core_site_width_micron = 0.0; 
    
    int die_x_min = 0, die_y_min = 0;
    int die_x_max = 0, die_y_max = 0;
};


struct Bin {
    std::map<int, std::vector<int>> cells_by_width;
    std::vector<int> all_cells_in_bin;
};

class BinGrid {
public:
    int num_bins_x, num_bins_y;
    int bin_width, bin_height;
    int die_x_min, die_y_min;
    
    std::vector<std::vector<Bin>> bins; 

    void init(int die_x_max, int die_y_max, int nx, int ny) {
        num_bins_x = nx;
        num_bins_y = ny;
        die_x_min = 0; 
        die_y_min = 0; 
        
        if (nx <= 0 || ny <= 0) {
             std::cerr << "[BinGrid] Error: nx/ny must be > 0" << std::endl;
             exit(1);
        }

        bin_width = (int)std::ceil((double)die_x_max / nx);
        bin_height = (int)std::ceil((double)die_y_max / ny);
        if (bin_width == 0) bin_width = 1;
        if (bin_height == 0) bin_height = 1;
        
        bins.resize(num_bins_x, std::vector<Bin>(num_bins_y));
        std::cout << "[BinGrid] Initialized " << nx << "x" << ny 
                  << " grid (Bin size: " << bin_width << "x" << bin_height << " DBU)" << std::endl;
    }

    std::pair<int, int> getBinIndex(int x, int y) {
        int idx_x = (x - die_x_min) / bin_width;
        int idx_y = (y - die_y_min) / bin_height;
        
        if (idx_x >= num_bins_x) idx_x = num_bins_x - 1;
        if (idx_y >= num_bins_y) idx_y = num_bins_y - 1;
        if (idx_x < 0) idx_x = 0;
        if (idx_y < 0) idx_y = 0;
        return {idx_x, idx_y};
    }

    void addInstance(const Inst& inst, int inst_id) {
        auto indices = getBinIndex(inst.x, inst.y);
        bins[indices.first][indices.second].cells_by_width[inst.macro_width].push_back(inst_id);
        bins[indices.first][indices.second].all_cells_in_bin.push_back(inst_id);
    }

    void removeInstance(const Inst& inst, int inst_id) {
        auto indices = getBinIndex(inst.x, inst.y);
        int width = inst.macro_width;
        
        if (bins[indices.first][indices.second].cells_by_width.count(width)) {
            auto& candidates = bins[indices.first][indices.second].cells_by_width.at(width);
            for (int i = 0; i < (int)candidates.size(); ++i) {
                if (candidates[i] == inst_id) {
                    candidates.erase(candidates.begin() + i);
                    break;
                }
            }
        }
        
        auto& all_candidates = bins[indices.first][indices.second].all_cells_in_bin;
        for (int i = 0; i < (int)all_candidates.size(); ++i) {
            if (all_candidates[i] == inst_id) {
                all_candidates.erase(all_candidates.begin() + i);
                break;
            }
        }
    }

    void clear() {
        for (int i = 0; i < num_bins_x; ++i) {
            for (int j = 0; j < num_bins_y; ++j) {
                bins[i][j].cells_by_width.clear();
                bins[i][j].all_cells_in_bin.clear(); 
            }
        }
    }
};