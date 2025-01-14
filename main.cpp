#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#undef byte

#include <iostream>
#include <filesystem>
#include <thread>
#include "InvertedIndex.h"
#include "HTTPServer.h"

using namespace std;
namespace fs = std::filesystem;


void start_http_server(int port, InvertedIndex& index) {
    HTTPServer http_server(port, index, 4);
    http_server.start();
}

int main() {
    InvertedIndex index(2);

    string directory_path = "./text_files";

    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        cerr << "Directory does not exist or is not valid: " << directory_path << endl;
        return 1;
    }
    index.update_index(directory_path);
    index.start_auto_update(directory_path, 20);
    thread http_server_thread(start_http_server, 8082, std::ref(index));
    http_server_thread.join();

    return 0;
}
