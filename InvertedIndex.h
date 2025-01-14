#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <array>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <set>
#include "ThreadPool.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

class ThreadPool;

class InvertedIndex {
public:
    explicit InvertedIndex(size_t thread_count);
    ~InvertedIndex();

    void add_document(const std::string& doc_id, const std::string& text);
    void remove_document(const std::string& doc_id);
    std::string query(const std::string& word);
    std::vector<std::string> get_first_indices(size_t count);
    void update_index(const std::string& directory);
    void start_auto_update(const std::string& directory, size_t interval);
    void stop_auto_update();

private:
    using DocMap = std::unordered_map<std::string, std::vector<int>>;
    using ShardMap = std::unordered_map<std::string, DocMap>;

    static constexpr size_t NUM_SHARDS = 8;

    ThreadPool thread_pool;
    std::array<ShardMap, NUM_SHARDS> shard_maps;
    std::array<std::shared_mutex, NUM_SHARDS> shard_mutexes;

    bool stop_updating = false;
    std::mutex update_mutex;
    std::condition_variable cv;
    std::thread update_thread;

    size_t get_shard_index(const std::string& key) const;
};
