#include "PathAllocator.hpp"
PathAllocator::PathAllocator(const std::vector<Die>& d, const std::vector<std::vector<int>>& cap)
    : dies(d), capacity(cap), total_dies(d.size()), total_pairs(0), successful_pairs(0) {
    usage.assign(total_dies, std::vector<int>(total_dies, 0));

    for (int i = 0; i < total_dies; i++) {
        for (int s : dies[i].s_points) {
            dies[i].s_set.insert(s);
        }
        for (int l : dies[i].l_points) {
            dies[i].l_set.insert(l);
        }
    }
}

int PathAllocator::find_die(int node, bool is_s_point) {
    for (int i = 0; i < total_dies; i++) {
        if (is_s_point && dies[i].s_set.count(node)) return i;
        if (!is_s_point && dies[i].l_set.count(node)) return i;
    }
    return -1;
}

std::vector<std::vector<int>> PathAllocator::find_paths(int source_die, int target_die, int max_hops) {
    std::string cache_key = std::to_string(source_die) + "_" + std::to_string(target_die) + "_" + std::to_string(max_hops);
    if (path_cache.count(cache_key)) {
        return path_cache[cache_key];
    }

    std::vector<std::vector<int>> all_paths;
    std::queue<std::vector<int>> q;
    q.push({source_die});

    while (!q.empty() && all_paths.size() < 1000) { // 限制路径数量
        std::vector<int> current_path = q.front();
        q.pop();

        int current_die = current_path.back();

        // 如果到达目标或路径太长
        if (current_die == target_die || current_path.size() >= max_hops) {
            all_paths.push_back(current_path);
            continue;
        }

        for (int next_die = 0; next_die < total_dies; next_die++) {
            if (next_die == current_die) continue;
            if (capacity[current_die][next_die] == 0) continue;

            // 避免环路
            bool has_cycle = false;
            for (int die : current_path) {
                if (die == next_die) {
                    has_cycle = true;
                    break;
                }
            }
            if (has_cycle) continue;

            std::vector<int> new_path = current_path;
            new_path.push_back(next_die);
            q.push(new_path);
        }
    }

    std::sort(all_paths.begin(), all_paths.end(),
         [](const std::vector<int>& a, const std::vector<int>& b) {
             return a.size() < b.size();
         });

    path_cache[cache_key] = all_paths;
    return all_paths;
}

bool PathAllocator::is_path_available(const std::vector<int>& die_path) {
    for (int i = 0; i < die_path.size() - 1; i++) {
        int from = die_path[i];
        int to = die_path[i + 1];
        if (usage[from][to] >= capacity[from][to]) {
            return false;
        }
    }
    return true;
}

void PathAllocator::use_path(const std::vector<int>& die_path) {
    for (int i = 0; i < die_path.size() - 1; i++) {
        int from = die_path[i];
        int to = die_path[i + 1];
        usage[from][to]++;
    }
}

int PathAllocator::select_intermediate_l(int die_id, const std::vector<int>& used_l) {
    for (int l : dies[die_id].l_points) {
        if (std::find(used_l.begin(), used_l.end(), l) == used_l.end()) {
            return l;
        }
    }
    return -1; // 没有可用的l点
}

bool PathAllocator::allocate_single_path(int s, int l, std::vector<Path>& paths) {
    int s_die = find_die(s, true);
    int l_die = find_die(l, false);

    if (s_die == -1 || l_die == -1) return false;
    if (s_die == l_die) return true;

    std::vector<std::vector<int>> possible_paths = find_paths(s_die, l_die);

    for (const auto& die_path : possible_paths) {
        if (is_path_available(die_path)) {
            std::vector<int> m_l;
            for (int i = 1; i < die_path.size() - 1; i++) {
                int intermediate_die = die_path[i];
                int l_point = select_intermediate_l(intermediate_die, m_l);
                if (l_point == -1) {
                    break;
                }
                m_l.push_back(l_point);
            }

            if (m_l.size() == die_path.size() - 2) {
                use_path(die_path);
                paths.push_back({s, l, die_path, m_l});
                return true;
            }
        }
    }

    return false;
}

std::vector<Path> PathAllocator::allocate_all_paths() {
    std::vector<Path> paths;
    auto start_time = std::chrono::high_resolution_clock::now();

    // 预计算总的s-l对数量
    for (int s_die = 0; s_die < total_dies; s_die++) {
        for (int l_die = 0; l_die < total_dies; l_die++) {
            if (s_die != l_die) {
                total_pairs += (long long)dies[s_die].s_points.size() * dies[l_die].l_points.size();
            }
        }
    }

    std::cout << total_pairs << " paths need alloting" << std::endl;

    long long processed = 0;

    for (int s_die = 0; s_die < total_dies; s_die++) {
        for (int l_die = 0; l_die < total_dies; l_die++) {
            if (s_die == l_die) continue;

            std::cout << " Die" << s_die << " -> Die" << l_die
                 << " (" << dies[s_die].s_points.size() << " s , "
                 << dies[l_die].l_points.size() << " l )" << std::endl;

            for (int s : dies[s_die].s_points) {
                for (int l : dies[l_die].l_points) {
                    if (allocate_single_path(s, l, paths)) {
                        successful_pairs++;
                    }
                    processed++;
                    // if (processed % 100000 == 0) {
                    //     auto current_time = std::chrono::high_resolution_clock::now();
                    //     auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
                    //     double progress = (double)processed / total_pairs * 100;
                    //     std::cout << "process: " << progress << "% (" << processed << "/" << total_pairs
                    //          << "), cost time: " << elapsed << "s: " << successful_pairs << std::endl;
                    // }
                }
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();

    std::cout << "time cost: " << total_time << std::endl;
    std::cout << "successful allocation: " << successful_pairs << "/" << total_pairs
         << " (" << (double)successful_pairs/total_pairs*100 << "%)" << std::endl;

    return paths;
}

std::vector<std::vector<int>> PathAllocator::get_usage() const {
    return usage;
}

void PathAllocator::print_statistics() const {
    std::cout << "\n=== total info ===" << std::endl;

    for (int i = 0; i < total_dies; i++) {
        std::cout << "Die" << i << ": " << dies[i].s_points.size()
             << " s , " << dies[i].l_points.size() << " l " << std::endl;
    }

    std::cout << "\n=== usage situation ===" << std::endl;
    int total_capacity = 0, total_used = 0;
    for (int i = 0; i < total_dies; i++) {
        for (int j = 0; j < total_dies; j++) {
            if (i != j && capacity[i][j] > 0) {
                total_capacity += capacity[i][j];
                total_used += usage[i][j];
                double utilization = (double)usage[i][j] / capacity[i][j] * 100;
                std::cout << "Die" << i << "->Die" << j << ": "
                     << usage[i][j] << "/" << capacity[i][j]
                     << " (" << utilization << "%)" << std::endl;
            }
        }
    }

    std::cout << "总体利用率: " << total_used << "/" << total_capacity
         << " (" << (double)total_used/total_capacity*100 << "%)" << std::endl;
}

std::vector<std::vector<std::string>> read_position_file(const std::string& filename) {
    std::vector<std::vector<std::string>> die_nodes;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open position file " << filename << std::endl;
        return die_nodes;
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string nodes_str = line.substr(colon_pos + 1);
        std::istringstream iss(nodes_str);
        std::string node;
        std::vector<std::string> nodes;

        while (iss >> node) {
            nodes.push_back(node);
        }
        die_nodes.push_back(nodes);
    }

    file.close();
    return die_nodes;
}

std::vector<std::vector<int>> read_network_file(const std::string& filename) {
    std::vector<std::vector<int>> network;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open network file " << filename << std::endl;
        return network;
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::vector<int> row;
        int value;

        while (iss >> value) {
            row.push_back(value);
        }

        if (!row.empty()) {
            network.push_back(row);
        }
    }

    file.close();
    return network;
}

std::map<std::string, std::pair<char, int>> read_sl_file(const std::string& filename) {
    std::map<std::string, std::pair<char, int>> node_types;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open s/l file " << filename << std::endl;
        return node_types;
    }

    int line_count = 0;
    int error_count = 0;

    while (std::getline(file, line)) {
        line_count++;
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string node, type;
        int weight = 1;

        if (!(iss >> node >> type)) {
            error_count++;
            if (error_count <= 5) {
                std::cerr << "Warning: Invalid line " << line_count << ": " << line << std::endl;
            }
            continue;
        }

        if (type == "s") {
            if (!(iss >> weight)) {
                weight = 1;
            }
        }

        if (type != "s" && type != "l") {
            error_count++;
            if (error_count <= 5) {
                std::cerr << "Warning: Unknown type '" << type << "' at line " << line_count << std::endl;
            }
            continue;
        }
        node_types[node] = std::make_pair(type[0], weight);
    }

    if (error_count > 5) {
        std::cerr << "... and " << (error_count - 5) << " more errors" << std::endl;
    }

    file.close();
    return node_types;
}

int parse_node_id(const std::string& node) {
    try {
        if (node.length() < 2 || node[0] != 'g') {
            std::cerr << "Warning: Invalid node format: " << node << std::endl;
            return -1;
        }

        std::string id_str = node.substr(1);
        if (id_str[0] == 'p') {
            return 10000 + std::stoi(id_str.substr(1));
        }

        for (char c : id_str) {
            if (!std::isdigit(c)) {
                std::cerr << "Warning: Non-numeric node ID: " << node << std::endl;
                return -1;
            }
        }

        return std::stoi(id_str);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to parse node ID: " << node << " - " << e.what() << std::endl;
        return -1;
    }
}

std::vector<Die> build_dies(const std::vector<std::vector<std::string>>& die_nodes,
                      const std::map<std::string, std::pair<char, int>>& node_types) {
    std::vector<Die> dies(die_nodes.size());
    int invalid_nodes = 0;

    for (int die_id = 0; die_id < die_nodes.size(); die_id++) {
        for (const std::string& node : die_nodes[die_id]) {
            auto it = node_types.find(node);
            if (it != node_types.end()) {
                int node_id = parse_node_id(node);
                if (node_id == -1) {
                    invalid_nodes++;
                    continue;
                }
                if (it->second.first == 's') {
                    dies[die_id].s_points.push_back(node_id);
                } else if (it->second.first == 'l') {
                    dies[die_id].l_points.push_back(node_id);
                }
            }
        }
    }

    if (invalid_nodes > 0) {
        std::cout << "Warning: Skipped " << invalid_nodes << " nodes with invalid IDs" << std::endl;
    }

    return dies;
}

void save_results(const std::vector<Path>& paths, const std::vector<std::vector<int>>& usage,
                 const std::string& output_file) {
    std::ofstream file(output_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create output file " << output_file << std::endl;
        return;
    }

    file << "=== 路径分配结果 ===" << std::endl;
    file << "总路径数: " << paths.size() << std::endl << std::endl;

    // TODO:写入前1000条路径信息
    file << "=== 路径详情 (前1000条) ===" << std::endl;
    for (int i = 0; i < std::min(1000, (int)paths.size()); i++) {
        const auto& path = paths[i];
        file << "s" << path.s_point << " -> l" << path.l_point << ": ";

        for (int j = 0; j < path.die_seq.size(); j++) {
            if (j > 0) file << " -> ";
            file << "D" << path.die_seq[j];
            if (j > 0 && j < path.die_seq.size() - 1 && j-1 < path.m_l.size()) {
                file << "(l" << path.m_l[j-1] << ")";
            }
        }
        file << std::endl;
    }

    if (paths.size() > 1000) {
        file << "还有 " << (paths.size() - 1000) << " 条路径" << std::endl;
    }

    file << std::endl << "=== Die间链路使用统计 ===" << std::endl;
    for (int i = 0; i < usage.size(); i++) {
        for (int j = 0; j < usage[i].size(); j++) {
            if (i != j && usage[i][j] > 0) {
                file << "D" << i << " -> D" << j << ": " << usage[i][j] << std::endl;
            }
        }
    }

    file.close();
    std::cout << "data save to: " << output_file << std::endl;
}