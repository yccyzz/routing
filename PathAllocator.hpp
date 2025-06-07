#ifndef PATH_ALLOCATOR_HPP
#define PATH_ALLOCATOR_HPP

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

struct Die {
    std::vector<int> s_points;
    std::vector<int> l_points;
    std::unordered_set<int> s_set;
    std::unordered_set<int> l_set;
};

struct Path {
    int s_point;
    int l_point;
    std::vector<int> die_seq;
    std::vector<int> m_l; // 中转的l点
};

class PathAllocator {
private:
    std::vector<Die> dies;
    std::vector<std::vector<int>> capacity; // die间容量限制
    std::vector<std::vector<int>> usage;    // die已使用次数
    int total_dies;

    std::unordered_map<std::string, std::vector<std::vector<int>>> path_cache;

    long long total_pairs;
    long long successful_pairs;

public:
    PathAllocator(const std::vector<Die>& d, const std::vector<std::vector<int>>& cap);
    ~PathAllocator() = default;
    int find_die(int node, bool is_s_point);

    // BFS找到source_die到target_die的所有可能路径
    std::vector<std::vector<int>> find_paths(int source_die, int target_die, int max_hops = 4);
    bool is_path_available(const std::vector<int>& die_path);
    void use_path(const std::vector<int>& die_path);
    int select_intermediate_l(int die_id, const std::vector<int>& used_l);
    bool allocate_single_path(int s, int l, std::vector<Path>& paths);
    std::vector<Path> allocate_all_paths();
    std::vector<std::vector<int>> get_usage() const;
    void print_statistics() const;
};

std::vector<std::vector<std::string>> read_position_file(const std::string& filename);
std::vector<std::vector<int>> read_network_file(const std::string& filename);
std::map<std::string, std::pair<char, int>> read_sl_file(const std::string& filename);
int parse_node_id(const std::string& node);
std::vector<Die> build_dies(const std::vector<std::vector<std::string>>& die_nodes,
                           const std::map<std::string, std::pair<char, int>>& node_types);
void save_results(const std::vector<Path>& paths, const std::vector<std::vector<int>>& usage,
                 const std::string& output_file);

#endif // PATH_ALLOCATOR_HPP