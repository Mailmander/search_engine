#include "InvertedIndex.h"


InvertedIndex::InvertedIndex(size_t thread_count): thread_pool(thread_count)
{
    std::cout << "InvertedIndex created with " << thread_count << " threads.\n";
}

InvertedIndex::~InvertedIndex() {
    stop_auto_update();
    std::cout << "InvertedIndex destroyed.\n";
}

size_t InvertedIndex::get_shard_index(const std::string& key) const {
    static std::hash<std::string> hasher;
    return hasher(key) % NUM_SHARDS;
}

void InvertedIndex::add_document(const std::string& doc_id, const std::string& text) {
    std::string document_name = std::filesystem::path(doc_id).filename().string();
    std::istringstream stream(text);
    std::string word;
    int position = 0;

    while (stream >> word) {
        word.erase(std::remove_if(word.begin(), word.end(),
                                  [](unsigned char c){ return std::ispunct(c); }),
                   word.end());
        std::transform(word.begin(), word.end(), word.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        if (!word.empty()) {
            size_t shard_idx = get_shard_index(word);
            std::unique_lock<std::shared_mutex> lock(shard_mutexes[shard_idx]);
            shard_maps[shard_idx][word][document_name].push_back(position);
            ++position;
        }
    }

    //std::cout << "Document \"" << document_name << "\" indexed.\n";
}

void InvertedIndex::remove_document(const std::string& doc_id) {
    std::string document_name = std::filesystem::path(doc_id).filename().string();
    for (size_t shard_idx = 0; shard_idx < NUM_SHARDS; ++shard_idx) {
        std::unique_lock<std::shared_mutex> lock(shard_mutexes[shard_idx]);
        auto& shard_map = shard_maps[shard_idx];
        for (auto it = shard_map.begin(); it != shard_map.end();) {
            it->second.erase(document_name);
            if (it->second.empty()) {
                it = shard_map.erase(it);
            } else {
                ++it;
            }
        }
    }
    //std::cout << "Document \"" << document_name << "\" removed.\n";
}

std::string InvertedIndex::query(const std::string& word) {
    size_t shard_idx = get_shard_index(word);
    std::shared_lock<std::shared_mutex> lock(shard_mutexes[shard_idx]);
    const auto& shard_map = shard_maps[shard_idx];
    auto it = shard_map.find(word);
    if (it == shard_map.end()) {
        return "Word not found\n";
    }
    std::ostringstream result;
    result << "Word: " << word << "\n";
    for (const auto& [doc_id, positions] : it->second) {
        result << "  Document: " << doc_id << ", Positions: ";
        for (int pos : positions) {
            result << pos << " ";
        }
        result << "\n";
    }
    return result.str();
}

void InvertedIndex::update_index(const std::string& directory) {
    thread_pool.wait_for_completion();

    std::vector<std::string> existing_files;
    {
        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(NUM_SHARDS);
        for (size_t i = 0; i < NUM_SHARDS; ++i) {
            locks.emplace_back(shard_mutexes[i]);
        }
        for (size_t i = 0; i < NUM_SHARDS; ++i) {
            for (const auto& [word, doc_map] : shard_maps[i]) {
                for (const auto& [doc_id, _positions] : doc_map) {
                    existing_files.push_back(doc_id);
                }
            }
        }
    }
    std::sort(existing_files.begin(), existing_files.end());
    existing_files.erase(std::unique(existing_files.begin(), existing_files.end()),
                         existing_files.end());

    std::set<std::string> current_files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            current_files.insert(entry.path().filename().string());
        }
    }

    for (const std::string& file : existing_files) {
        if (current_files.find(file) == current_files.end()) {
            remove_document(file);
        }
    }

    for (const std::string& file : current_files) {
        if (!std::binary_search(existing_files.begin(), existing_files.end(), file)) {
            thread_pool.enqueue([this, directory, file]() {
                auto full_path = std::filesystem::path(directory) / file;
                std::ifstream input(full_path);
                if (input.is_open()) {
                    std::ostringstream buffer;
                    buffer << input.rdbuf();
                    add_document(file, buffer.str());
                } else {
                    std::cerr << "Error opening file: " << file << "\n";
                }
            });
        }
    }

    thread_pool.wait_for_completion();
    std::cout << "Index has been successfully updated.\n";
}

void InvertedIndex::start_auto_update(const std::string& directory, size_t interval)
{
    stop_updating = false;
    update_thread = std::thread([this, directory, interval]() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(update_mutex);
                if (cv.wait_for(lock, std::chrono::seconds(interval), [this] { return stop_updating; })) {
                    break;
                }
            }
            update_index(directory);
        }
    });
}

void InvertedIndex::stop_auto_update() {
    {
        std::lock_guard<std::mutex> lock(update_mutex);
        stop_updating = true;
    }
    cv.notify_all();
    if (update_thread.joinable()) {
        update_thread.join();
    }
}
