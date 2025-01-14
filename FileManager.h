#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <string>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <iostream>

class FileManager {
private:
    std::mutex file_mutex;

public:
    bool write_file(const std::string& file_path, const std::string& content);
    bool delete_file(const std::string& file_path);
    std::string read_file(const std::string& file_path);
};

#endif
