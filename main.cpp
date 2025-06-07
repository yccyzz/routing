#include "PathAllocator.hpp"

int main() {
    std::string position_file = "C:/Users/dell/Downloads/design.die.position";
    std::string network_file = "C:/Users/dell/Downloads/design.die.network";
    std::string sl_file = "C:/Users/dell/Downloads/design.net";
    std::string output_file = "path_allocation_results.txt";

    std::vector<std::vector<std::string>> die_nodes = read_position_file(position_file);
    if (die_nodes.empty()) {
        std::cerr << "fail to catch ./position" << std::endl;
        return 1;
    }
    std::cout << "node num: " << die_nodes.size() << std::endl;

    std::vector<std::vector<int>> capacity = read_network_file(network_file);
    if (capacity.empty()) {
        std::cerr << "fail to catch ./network" << std::endl;
        return 1;
    }
    std::cout << "capacity.size: " << capacity.size() << "x" << capacity[0].size() << std::endl;

    std::map<std::string, std::pair<char, int>> node_types = read_sl_file(sl_file);
    if (node_types.empty()) {
        std::cerr << "fail to catch ./net" << std::endl;
        return 1;
    }
    std::cout << "node_types num : " << node_types.size() << std::endl;

    std::vector<Die> dies = build_dies(die_nodes, node_types);
    if (dies.size() != capacity.size()) {
        std::cerr << "fail to match dies_num to capacity_num" << std::endl;
        return 1;
    }
    PathAllocator allocator(dies, capacity);
    std::vector<Path> paths = allocator.allocate_all_paths();

    allocator.print_statistics();
    save_results(paths, allocator.get_usage(), output_file);

    return 0;
}