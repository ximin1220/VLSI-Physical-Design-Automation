#pragma once
#include <fstream>
#include <sstream>
#include <iostream>


static bool starts_with(const std::string &s, const std::string &prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

void parseDEF(const std::string &def_path, DesignDB &db) {
    std::ifstream fin(def_path);
    if (!fin) {
        std::cerr << "Cannot open DEF: " << def_path << "\n";
        exit(1);
    }

    std::string line;
    bool in_components = false;
    bool in_nets = false;
    bool in_pins = false;

    Inst cur_inst;
    bool pending_inst = false;

    Net cur_net;
    bool pending_net = false;


    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string tok;


        if (!(ss >> tok)) continue;






        if (tok == "UNITS") {

            std::string t2;
            while (ss >> t2) {
                if (t2 == "DISTANCE") {
                    std::string t3; int n;
                    ss >> t3 >> n;
                    db.def_dbu_per_micron = n;
                    break;
                }
            }
        }

        else if (tok == "ROW") {

            Row row;
            ss >> row.name >> row.site_name >> row.x >> row.y >> row.orient;
            std::string tmp;
            int do_cnt, by_cnt;
            ss >> tmp >> do_cnt >> tmp >> by_cnt >> tmp >> row.step_x;
            int step_y;
            ss >> step_y;
            row.site_count = do_cnt;
            db.rows.push_back(row);
        }

        else if (tok == "PINS") {
            in_pins = true;
            continue;
        } else if (tok == "END" && in_pins) {
            std::string sec;
            ss >> sec;
            if (sec == "PINS") {
                in_pins = false;
            }
        }

        else if (tok == "COMPONENTS") {
            in_components = true;
            continue;
        } else if (tok == "END" && in_components) {
            std::string sec;
            ss >> sec;
            if (sec == "COMPONENTS") {
                in_components = false;




                if (pending_inst) {
                    int id = (int)db.instances.size();
                    db.inst_name_to_id[cur_inst.name] = id;
                    db.instances.push_back(cur_inst);
                    pending_inst = false;
                }
            }
        }

        else if (tok == "NETS") {
            in_nets = true;
            continue;
        } else if (tok == "END" && in_nets) {
            std::string sec;
            ss >> sec;
            if (sec == "NETS") {
                in_nets = false;




                if (pending_net) {
                    db.net_name_to_id[cur_net.name] = (int)db.nets.size();
                    db.nets.push_back(cur_net);
                    pending_net = false;
                }
            }
        }


        else if (in_pins && tok == "-") {

            IOPin pin;
            ss >> pin.name;
            bool placed_found = false;

            {
                std::string t2;
                while (ss >> t2) {
                    if (t2 == "+") {
                        std::string t3;
                        if (!(ss >> t3)) break;
                        if (t3 == "PLACED") {
                            std::string lp, rp;
                            ss >> lp >> pin.x >> pin.y >> rp >> pin.orient;
                            placed_found = true;
                            break;
                        }
                    }
                }
            }

            while (!placed_found && std::getline(fin, line)) {
                std::stringstream s2(line);
                std::string t2;
                if (!(s2 >> t2)) continue;
                if (t2 == "+") {
                    std::string t3;
                    if (!(s2 >> t3)) continue;
                    if (t3 == "PLACED") {
                        std::string lp, rp;
                        s2 >> lp >> pin.x >> pin.y >> rp >> pin.orient;
                        placed_found = true;
                    }
                }
                if (line.find(';') != std::string::npos) {
                    break;
                }
            }
            db.io_pins[pin.name] = pin;
        }

        else if (tok == "DIEAREA") {
            std::string lp1, rp1, lp2, rp2;
            ss >> lp1 >> db.die_x_min >> db.die_y_min >> rp1;
            ss >> lp2 >> db.die_x_max >> db.die_y_max >> rp2;


            db.die_x_min = 0;
            db.die_y_min = 0;

            std::cout << "[DEF] Parsed DIEAREA: (0, 0) to ("
                      << db.die_x_max << ", " << db.die_y_max << ")" << std::endl;
        }



        else if (tok == "COMPONENTS") {
            in_components = true;
            continue;
        } else if (tok == "END" && in_components) {
            std::string sec;
            ss >> sec;
            if (sec == "COMPONENTS") {
                in_components = false;




                if (pending_inst) {
                    int id = (int)db.instances.size();
                    db.inst_name_to_id[cur_inst.name] = id;
                    db.instances.push_back(cur_inst);
                    pending_inst = false;
                }
            }
        }



        else if (in_components && tok == "-") {

            if (pending_inst) {
                int id = (int)db.instances.size();
                db.inst_name_to_id[cur_inst.name] = id;
                db.instances.push_back(cur_inst);

            }


            cur_inst = Inst();
            ss >> cur_inst.name >> cur_inst.macro_name;
            pending_inst = true;


            std::string tok_inner;
            while (ss >> tok_inner) {
                std::string status;
                if (tok_inner == "+") {
                    ss >> status;
                } else if (starts_with(tok_inner, "+")) {
                    status = tok_inner.substr(1);
                } else {
                    continue;
                }

                if (status == "PLACED" || status == "FIXED") {
                    if (status == "FIXED") cur_inst.is_fixed = true;
                    std::string lparen, rparen;
                    ss >> lparen >> cur_inst.x >> cur_inst.y >> rparen >> cur_inst.orient;
                    break;
                }
            }
        }
        else if (in_components && pending_inst &&
                 (tok == "+" || starts_with(tok, "+"))) {









            std::string status;
            if (tok == "+") {
                ss >> status;
            } else {
                status = tok.substr(1);
            }

            if (status == "PLACED" || status == "FIXED") {
                if (status == "FIXED") cur_inst.is_fixed = true;
                std::string lparen, rparen;
                ss >> lparen >> cur_inst.x >> cur_inst.y >> rparen >> cur_inst.orient;
            }
        }




        else if (in_nets && tok == "-") {

            if (pending_net) {
                db.net_name_to_id[cur_net.name] = (int)db.nets.size();
                db.nets.push_back(cur_net);
                pending_net = false;
            }
            cur_net = Net();
            ss >> cur_net.name;
            pending_net = true;


            std::string tok_inner;
            while (ss >> tok_inner) {
                if (tok_inner == "(") {
                    std::string name1, name2, rparen;
                    ss >> name1 >> name2 >> rparen;
                    NetPin pin;
                    if (name1 == "PIN") {
                        pin.is_port = true; pin.port_name = name2;
                    } else {
                        pin.is_port = false;
                        auto it = db.inst_name_to_id.find(name1);
                        pin.inst_id = (it != db.inst_name_to_id.end()) ? it->second : -1;
                        pin.pin_name = name2;
                    }
                    cur_net.pins.push_back(pin);
                }
            }
        }
        else if (in_nets && pending_net && tok == "(") {



            {
                std::string name1, name2, rparen;
                ss >> name1 >> name2 >> rparen;
                NetPin pin;
                if (name1 == "PIN") {
                    pin.is_port = true; pin.port_name = name2;
                } else {
                    pin.is_port = false;
                    auto it = db.inst_name_to_id.find(name1);
                    pin.inst_id = (it != db.inst_name_to_id.end()) ? it->second : -1;
                    pin.pin_name = name2;
                }
                cur_net.pins.push_back(pin);
            }


            std::string tok_inner;
            while (ss >> tok_inner) {
                if (tok_inner == "(") {
                    std::string name1, name2, rparen;
                    ss >> name1 >> name2 >> rparen;
                    NetPin pin;
                    if (name1 == "PIN") {
                        pin.is_port = true; pin.port_name = name2;
                    } else {
                        pin.is_port = false;
                        auto it = db.inst_name_to_id.find(name1);
                        pin.inst_id = (it != db.inst_name_to_id.end()) ? it->second : -1;
                        pin.pin_name = name2;
                    }
                    cur_net.pins.push_back(pin);
                }
            }
        }
        else if (in_nets && pending_net && tok == ";") {





        }

    }




    /*
    if (pending_inst) {
        int id = (int)db.instances.size();
        db.inst_name_to_id[cur_inst.name] = id;
        db.instances.push_back(cur_inst);
    }
    if (pending_net) {
        db.net_name_to_id[cur_net.name] = (int)db.nets.size();
        db.nets.push_back(cur_net);
    }
    */

    std::cerr << "[DEF] dbu_per_micron=" << db.def_dbu_per_micron
              << " rows=" << db.rows.size()
              << " insts=" << db.instances.size()
              << " nets=" << db.nets.size()
              << " io_pins=" << db.io_pins.size()
              << "\n";
}