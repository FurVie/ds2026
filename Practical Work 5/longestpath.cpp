#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

struct ReducerResult {
    int max_len = 0;
    std::vector<std::string> paths;
};

void mapper_worker(const std::vector<std::string>& all_paths,
                   size_t begin, size_t end,
                   std::vector<std::pair<int, std::string>>& out_pairs) {
    for (size_t i = begin; i < end; ++i) {
        const std::string& p = all_paths[i];
        int len = static_cast<int>(p.size());
        out_pairs.emplace_back(len, p);
    }
}

void reducer_worker(const std::vector<std::pair<int, std::string>>& in_pairs,
                    ReducerResult& out_result) {
    int local_max = 0;
    std::vector<std::string> local_paths;
    for (const auto& kv : in_pairs) {
        int len = kv.first;
        const std::string& path = kv.second;
        if (len > local_max) {
            local_max = len;
            local_paths.clear();
            local_paths.push_back(path);
        } else if (len == local_max) {
            local_paths.push_back(path);
        }
    }
    out_result.max_len = local_max;
    out_result.paths = std::move(local_paths);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <output_file> <input1> [input2 ...] \n";
        return 1;
    }

    std::string output_file = argv[1];
    std::vector<std::string> input_files;
    for (int i = 2; i < argc; ++i) {
        input_files.push_back(argv[i]);
    }

    std::vector<std::string> all_paths;
    for (const auto& fname : input_files) {
        std::ifstream in(fname);
        if (!in.is_open()) {
            std::cerr << "Cannot open input file: " << fname << "\n";
            return 1;
        }
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty())
                all_paths.push_back(line);
        }
        in.close();
    }

    if (all_paths.empty()) {
        std::cerr << "No paths found in input files.\n";
        return 1;
    }

    int num_mappers = 4;
    int num_reducers = 2;
    if (static_cast<int>(all_paths.size()) < num_mappers) {
        num_mappers = static_cast<int>(all_paths.size());
    }
    if (num_reducers > num_mappers) {
        num_reducers = num_mappers;
    }

    std::vector<std::thread> mapper_threads;
    std::vector<std::vector<std::pair<int, std::string>>> mapper_outputs(num_mappers);

    size_t total = all_paths.size();
    size_t base_chunk = total / num_mappers;
    size_t extra = total % num_mappers;
    size_t start = 0;

    for (int i = 0; i < num_mappers; ++i) {
        size_t chunk = base_chunk + (i < static_cast<int>(extra) ? 1 : 0);
        size_t end = start + chunk;
        mapper_threads.emplace_back(mapper_worker,
                                    std::cref(all_paths),
                                    start, end,
                                    std::ref(mapper_outputs[i]));
        start = end;
    }

    for (auto& t : mapper_threads) t.join();

    std::vector<std::vector<std::pair<int, std::string>>> reducer_inputs(num_reducers);
    for (const auto& m_out : mapper_outputs) {
        for (const auto& kv : m_out) {
            int len = kv.first;
            size_t h = std::hash<int>{}(len);
            int rid = static_cast<int>(h % num_reducers);
            reducer_inputs[rid].push_back(kv);
        }
    }

    std::vector<std::thread> reducer_threads;
    std::vector<ReducerResult> reducer_results(num_reducers);

    for (int r = 0; r < num_reducers; ++r) {
        reducer_threads.emplace_back(reducer_worker,
                                     std::cref(reducer_inputs[r]),
                                     std::ref(reducer_results[r]));
    }

    for (auto& t : reducer_threads) t.join();

    int global_max = 0;
    std::vector<std::string> longest_paths;

    for (const auto& res : reducer_results) {
        if (res.max_len > global_max) {
            global_max = res.max_len;
            longest_paths = res.paths;
        } else if (res.max_len == global_max) {
            longest_paths.insert(longest_paths.end(),
                                 res.paths.begin(), res.paths.end());
        }
    }

    if (global_max == 0) {
        std::cerr << "Could not compute longest path length.\n";
        return 1;
    }

    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Cannot open output file: " << output_file << "\n";
        return 1;
    }

    for (const auto& p : longest_paths) {
        if (static_cast<int>(p.size()) == global_max)
            out << global_max << " " << p << "\n";
    }
    out.close();

    std::cout << "Longest path length: " << global_max
              << ", number of paths: " << longest_paths.size() << "\n";
    std::cout << "Results written to: " << output_file << "\n";

    return 0;
}
