#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using MapperOutput = std::unordered_map<std::string, int>;
using ReducerOutput = std::unordered_map<std::string, int>;

std::mutex io_mutex;


std::string normalize(const std::string &w) {
    std::string res;
    for (char c : w) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            res.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return res;
}


std::vector<std::string> split_words(const std::string &text) {
    std::vector<std::string> words;
    std::string current;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                std::string norm = normalize(current);
                if (!norm.empty()) words.push_back(norm);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        std::string norm = normalize(current);
        if (!norm.empty()) words.push_back(norm);
    }
    return words;
}


void mapper_worker(const std::string &chunk, MapperOutput &out_map) {
    auto words = split_words(chunk);
    for (const auto &w : words) {
        ++out_map[w];
    }
}


void reducer_worker(const std::vector<std::pair<std::string, int>> &pairs,
                    ReducerOutput &out_map) {
    for (const auto &p : pairs) {
        out_map[p.first] += p.second;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input_file> <output_file> [num_mappers] [num_reducers]\n";
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];
    int num_mappers = (argc >= 4) ? std::stoi(argv[3]) : 4;
    int num_reducers = (argc >= 5) ? std::stoi(argv[4]) : 2;

    if (num_mappers <= 0 || num_reducers <= 0) {
        std::cerr << "Number of mappers and reducers must be > 0\n";
        return 1;
    }

   
    std::ifstream in(input_file, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Cannot open input file: " << input_file << "\n";
        return 1;
    }
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    in.close();

    if (text.empty()) {
        std::cerr << "Input file is empty.\n";
        return 1;
    }

    
    std::vector<std::thread> mapper_threads;
    std::vector<MapperOutput> mapper_outputs(num_mappers);

    size_t chunk_size = text.size() / num_mappers;
    size_t start = 0;

    for (int i = 0; i < num_mappers; ++i) {
        size_t end = (i == num_mappers - 1) ? text.size() : start + chunk_size;

        
        if (end < text.size()) {
            while (end < text.size() &&
                   !std::isspace(static_cast<unsigned char>(text[end]))) {
                ++end;
            }
        }

        std::string chunk = text.substr(start, end - start);
        mapper_threads.emplace_back(mapper_worker, chunk,
                                    std::ref(mapper_outputs[i]));
        start = end;
    }

    for (auto &t : mapper_threads) t.join();

    
    std::vector<std::vector<std::pair<std::string, int>>> reducer_inputs(
        num_reducers);

    for (const auto &m_out : mapper_outputs) {
        for (const auto &kv : m_out) {
            const std::string &word = kv.first;
            int count = kv.second;
            std::size_t h = std::hash<std::string>{}(word);
            int rid = static_cast<int>(h % num_reducers);
            reducer_inputs[rid].emplace_back(word, count);
        }
    }

    
    std::vector<std::thread> reducer_threads;
    std::vector<ReducerOutput> reducer_outputs(num_reducers);

    for (int r = 0; r < num_reducers; ++r) {
        reducer_threads.emplace_back(reducer_worker,
                                     std::cref(reducer_inputs[r]),
                                     std::ref(reducer_outputs[r]));
    }

    for (auto &t : reducer_threads) t.join();

    
    std::unordered_map<std::string, int> final_counts;
    for (const auto &r_out : reducer_outputs) {
        for (const auto &kv : r_out) {
            final_counts[kv.first] += kv.second;
        }
    }

    
    std::vector<std::pair<std::string, int>> sorted(final_counts.begin(),
                                                    final_counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](auto &a, auto &b) { return a.first < b.first; });

    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Cannot open output file: " << output_file << "\n";
        return 1;
    }

    for (const auto &kv : sorted) {
        out << kv.first << " " << kv.second << "\n";
    }
    out.close();

    std::cout << "Word count written to: " << output_file << "\n";
    return 0;
}
