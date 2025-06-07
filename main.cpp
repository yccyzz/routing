#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <chrono>
#include <memory>
using namespace std;
using namespace std::chrono;

struct Die {
    vector<int> s_points;
    vector<int> l_points;
    unordered_set<int> s_set; // 快速查找
    unordered_set<int> l_set;
};

struct Path {
    int s_point;
    int l_point;
    vector<int> die_seq;
    vector<int> intermediate_l; // 中转的l点
};

class PathAllocator {
private:
    vector<Die> dies;
    vector<vector<int>> capacity; // die间的容量限制
    vector<vector<int>> usage;    // die间的使用次数
    int total_dies;

    // 路径缓存，避免重复计算
    unordered_map<string, vector<vector<int>>> path_cache;

    // 统计信息
    long long total_pairs = 0;
    long long successful_pairs = 0;

public:
    PathAllocator(const vector<Die>& d, const vector<vector<int>>& cap)
        : dies(d), capacity(cap), total_dies(d.size()) {
        usage.assign(total_dies, vector<int>(total_dies, 0));

        // 构建快速查找集合
        for (int i = 0; i < total_dies; i++) {
            for (int s : dies[i].s_points) {
                dies[i].s_set.insert(s);
            }
            for (int l : dies[i].l_points) {
                dies[i].l_set.insert(l);
            }
        }
    }

    // 找到节点所在的die
    int find_die(int node, bool is_s_point) {
        for (int i = 0; i < total_dies; i++) {
            if (is_s_point && dies[i].s_set.count(node)) return i;
            if (!is_s_point && dies[i].l_set.count(node)) return i;
        }
        return -1;
    }

    // 使用BFS找到从source_die到target_die的所有可能路径
    vector<vector<int>> find_paths(int source_die, int target_die, int max_hops = 4) {
        string cache_key = to_string(source_die) + "_" + to_string(target_die) + "_" + to_string(max_hops);
        if (path_cache.count(cache_key)) {
            return path_cache[cache_key];
        }

        vector<vector<int>> all_paths;
        queue<vector<int>> q;
        q.push({source_die});

        while (!q.empty() && all_paths.size() < 1000) { // 限制路径数量防止爆炸
            vector<int> current_path = q.front();
            q.pop();

            int current_die = current_path.back();

            // 如果到达目标
            if (current_die == target_die) {
                all_paths.push_back(current_path);
                continue;
            }

            // 如果路径太长，跳过
            if (current_path.size() >= max_hops) continue;

            // 探索邻居
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

                vector<int> new_path = current_path;
                new_path.push_back(next_die);
                q.push(new_path);
            }
        }

        // 按路径长度排序，优先使用短路径
        sort(all_paths.begin(), all_paths.end(),
             [](const vector<int>& a, const vector<int>& b) {
                 return a.size() < b.size();
             });

        path_cache[cache_key] = all_paths;
        return all_paths;
    }

    // 检查路径是否可用
    bool is_path_available(const vector<int>& die_path) {
        for (int i = 0; i < die_path.size() - 1; i++) {
            int from = die_path[i];
            int to = die_path[i + 1];
            if (usage[from][to] >= capacity[from][to]) {
                return false;
            }
        }
        return true;
    }

    // 使用路径（更新使用计数）
    void use_path(const vector<int>& die_path) {
        for (int i = 0; i < die_path.size() - 1; i++) {
            int from = die_path[i];
            int to = die_path[i + 1];
            usage[from][to]++;
        }
    }

    // 选择中转l点
    int select_intermediate_l(int die_id, const vector<int>& used_l) {
        for (int l : dies[die_id].l_points) {
            if (find(used_l.begin(), used_l.end(), l) == used_l.end()) {
                return l;
            }
        }
        return -1; // 没有可用的l点
    }

    // 为单个s-l对分配路径
    bool allocate_single_path(int s, int l, vector<Path>& paths) {
        int s_die = find_die(s, true);
        int l_die = find_die(l, false);

        if (s_die == -1 || l_die == -1) return false;
        if (s_die == l_die) return true; // 同die，无需连接

        vector<vector<int>> possible_paths = find_paths(s_die, l_die);

        for (const auto& die_path : possible_paths) {
            if (is_path_available(die_path)) {
                // 选择中转l点
                vector<int> intermediate_l;
                for (int i = 1; i < die_path.size() - 1; i++) {
                    int intermediate_die = die_path[i];
                    int l_point = select_intermediate_l(intermediate_die, intermediate_l);
                    if (l_point == -1) {
                        // 没有足够的中转l点，尝试下一条路径
                        break;
                    }
                    intermediate_l.push_back(l_point);
                }

                // 如果中转点数量正确，使用这条路径
                if (intermediate_l.size() == die_path.size() - 2) {
                    use_path(die_path);
                    paths.push_back({s, l, die_path, intermediate_l});
                    return true;
                }
            }
        }

        return false;
    }

    // 主要的路径分配函数
    vector<Path> allocate_all_paths() {
        vector<Path> paths;
        auto start_time = high_resolution_clock::now();

        cout << "开始路径分配..." << endl;

        // 预计算总的s-l对数量
        for (int s_die = 0; s_die < total_dies; s_die++) {
            for (int l_die = 0; l_die < total_dies; l_die++) {
                if (s_die != l_die) {
                    total_pairs += (long long)dies[s_die].s_points.size() * dies[l_die].l_points.size();
                }
            }
        }

        cout << "总共需要分配 " << total_pairs << " 条路径" << endl;

        long long processed = 0;

        // 按die对进行处理，提高局部性
        for (int s_die = 0; s_die < total_dies; s_die++) {
            for (int l_die = 0; l_die < total_dies; l_die++) {
                if (s_die == l_die) continue;

                cout << "处理 Die" << s_die << " -> Die" << l_die
                     << " (" << dies[s_die].s_points.size() << " s点, "
                     << dies[l_die].l_points.size() << " l点)" << endl;

                for (int s : dies[s_die].s_points) {
                    for (int l : dies[l_die].l_points) {
                        if (allocate_single_path(s, l, paths)) {
                            successful_pairs++;
                        }
                        processed++;

                        // 定期输出进度
                        if (processed % 100000 == 0) {
                            auto current_time = high_resolution_clock::now();
                            auto elapsed = duration_cast<seconds>(current_time - start_time).count();
                            double progress = (double)processed / total_pairs * 100;
                            cout << "进度: " << progress << "% (" << processed << "/" << total_pairs
                                 << "), 耗时: " << elapsed << "s, 成功: " << successful_pairs << endl;
                        }
                    }
                }
            }
        }

        auto end_time = high_resolution_clock::now();
        auto total_time = duration_cast<seconds>(end_time - start_time).count();

        cout << "路径分配完成!" << endl;
        cout << "总耗时: " << total_time << " 秒" << endl;
        cout << "成功分配: " << successful_pairs << "/" << total_pairs
             << " (" << (double)successful_pairs/total_pairs*100 << "%)" << endl;

        return paths;
    }

    // 获取使用统计
    vector<vector<int>> get_usage() const {
        return usage;
    }

    // 打印详细统计
    void print_statistics() const {
        cout << "\n=== 统计信息 ===" << endl;

        // Die信息
        for (int i = 0; i < total_dies; i++) {
            cout << "Die" << i << ": " << dies[i].s_points.size()
                 << " s点, " << dies[i].l_points.size() << " l点" << endl;
        }

        // 容量vs使用情况
        cout << "\n=== 链路使用情况 ===" << endl;
        int total_capacity = 0, total_used = 0;
        for (int i = 0; i < total_dies; i++) {
            for (int j = 0; j < total_dies; j++) {
                if (i != j && capacity[i][j] > 0) {
                    total_capacity += capacity[i][j];
                    total_used += usage[i][j];
                    double utilization = (double)usage[i][j] / capacity[i][j] * 100;
                    cout << "Die" << i << "->Die" << j << ": "
                         << usage[i][j] << "/" << capacity[i][j]
                         << " (" << utilization << "%)" << endl;
                }
            }
        }

        cout << "总体利用率: " << total_used << "/" << total_capacity
             << " (" << (double)total_used/total_capacity*100 << "%)" << endl;
    }
};

// 文件读取函数（保持原有的实现）
vector<vector<string>> read_position_file(const string& filename) {
    vector<vector<string>> die_nodes;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        cerr << "Error: Cannot open position file " << filename << endl;
        return die_nodes;
    }

    while (getline(file, line)) {
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos == string::npos) continue;

        string nodes_str = line.substr(colon_pos + 1);
        istringstream iss(nodes_str);
        string node;
        vector<string> nodes;

        while (iss >> node) {
            nodes.push_back(node);
        }
        die_nodes.push_back(nodes);
    }

    file.close();
    return die_nodes;
}

vector<vector<int>> read_network_file(const string& filename) {
    vector<vector<int>> network;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        cerr << "Error: Cannot open network file " << filename << endl;
        return network;
    }

    while (getline(file, line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        vector<int> row;
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

map<string, pair<char, int>> read_sl_file(const string& filename) {
    map<string, pair<char, int>> node_types;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        cerr << "Error: Cannot open s/l file " << filename << endl;
        return node_types;
    }

    int line_count = 0;
    int error_count = 0;

    while (getline(file, line)) {
        line_count++;
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        string node, type;
        int weight = 1;

        if (!(iss >> node >> type)) {
            error_count++;
            if (error_count <= 5) {
                cerr << "Warning: Invalid line " << line_count << ": " << line << endl;
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
                cerr << "Warning: Unknown type '" << type << "' at line " << line_count << endl;
            }
            continue;
        }

        node_types[node] = make_pair(type[0], weight);
    }

    if (error_count > 5) {
        cerr << "... and " << (error_count - 5) << " more errors" << endl;
    }

    file.close();
    return node_types;
}

int parse_node_id(const string& node) {
    try {
        if (node.length() < 2 || node[0] != 'g') {
            cerr << "Warning: Invalid node format: " << node << endl;
            return -1;
        }

        string id_str = node.substr(1);

        if (id_str[0] == 'p') {
            return 10000 + stoi(id_str.substr(1));
        }

        for (char c : id_str) {
            if (!isdigit(c)) {
                cerr << "Warning: Non-numeric node ID: " << node << endl;
                return -1;
            }
        }

        return stoi(id_str);
    } catch (const exception& e) {
        cerr << "Warning: Failed to parse node ID: " << node << " - " << e.what() << endl;
        return -1;
    }
}

vector<Die> build_dies(const vector<vector<string>>& die_nodes,
                      const map<string, pair<char, int>>& node_types) {
    vector<Die> dies(die_nodes.size());
    int invalid_nodes = 0;

    for (int die_id = 0; die_id < die_nodes.size(); die_id++) {
        for (const string& node : die_nodes[die_id]) {
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
        cout << "Warning: Skipped " << invalid_nodes << " nodes with invalid IDs" << endl;
    }

    return dies;
}

void save_results(const vector<Path>& paths, const vector<vector<int>>& usage,
                 const string& output_file) {
    ofstream file(output_file);
    if (!file.is_open()) {
        cerr << "Error: Cannot create output file " << output_file << endl;
        return;
    }

    file << "=== 路径分配结果 ===" << endl;
    file << "总路径数: " << paths.size() << endl << endl;

    // 写入路径信息（只写入前1000条，避免文件过大）
    file << "=== 路径详情 (前1000条) ===" << endl;
    for (int i = 0; i < min(1000, (int)paths.size()); i++) {
        const auto& path = paths[i];
        file << "s" << path.s_point << " -> l" << path.l_point << ": ";

        for (int j = 0; j < path.die_seq.size(); j++) {
            if (j > 0) file << " -> ";
            file << "D" << path.die_seq[j];
            if (j > 0 && j < path.die_seq.size() - 1 && j-1 < path.intermediate_l.size()) {
                file << "(l" << path.intermediate_l[j-1] << ")";
            }
        }
        file << endl;
    }

    if (paths.size() > 1000) {
        file << "... 还有 " << (paths.size() - 1000) << " 条路径" << endl;
    }

    // 写入使用统计
    file << endl << "=== Die间链路使用统计 ===" << endl;
    for (int i = 0; i < usage.size(); i++) {
        for (int j = 0; j < usage[i].size(); j++) {
            if (i != j && usage[i][j] > 0) {
                file << "D" << i << " -> D" << j << ": " << usage[i][j] << endl;
            }
        }
    }

    file.close();
    cout << "结果已保存到: " << output_file << endl;
}

int main() {
    string position_file = "C:/Users/dell/Downloads/design.die.position";
    string network_file = "C:/Users/dell/Downloads/design.die.network";
    string sl_file = "C:/Users/dell/Downloads/design.net";
    string output_file = "path_allocation_results.txt";

    cout << "=== 大规模路径分配程序 ===" << endl;
    cout << "加载数据文件..." << endl;

    // 读取数据
    vector<vector<string>> die_nodes = read_position_file(position_file);
    if (die_nodes.empty()) {
        cerr << "无法读取position文件" << endl;
        return 1;
    }
    cout << "找到 " << die_nodes.size() << " 个die" << endl;

    vector<vector<int>> capacity = read_network_file(network_file);
    if (capacity.empty()) {
        cerr << "无法读取network文件" << endl;
        return 1;
    }
    cout << "网络矩阵大小: " << capacity.size() << "x" << capacity[0].size() << endl;

    map<string, pair<char, int>> node_types = read_sl_file(sl_file);
    if (node_types.empty()) {
        cerr << "无法读取s/l定义文件" << endl;
        return 1;
    }
    cout << "找到 " << node_types.size() << " 个节点定义" << endl;

    // 构建数据结构
    vector<Die> dies = build_dies(die_nodes, node_types);

    // 验证数据
    if (dies.size() != capacity.size()) {
        cerr << "Die数量与网络矩阵大小不匹配" << endl;
        return 1;
    }

    // 创建路径分配器并执行分配
    PathAllocator allocator(dies, capacity);
    vector<Path> paths = allocator.allocate_all_paths();

    // 输出统计信息
    allocator.print_statistics();

    // 保存结果
    save_results(paths, allocator.get_usage(), output_file);

    return 0;
}