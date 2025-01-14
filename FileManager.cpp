#include "FileManager.h"
#include <sstream>

bool FileManager::write_file(const std::string& file_path, const std::string& content) {
    std::lock_guard<std::mutex> lock(file_mutex);

    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << file_path << std::endl;
        return false;
    }

    file << content;
    file.close();
    return true;
}

bool FileManager::delete_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(file_mutex);

    if (!std::filesystem::remove(file_path)) {
        std::cerr << "Failed to delete file: " << file_path << std::endl;
        return false;
    }

    return true;
}

std::string FileManager::read_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(file_mutex);

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << file_path << std::endl;
        return "";
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}
