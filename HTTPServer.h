#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#undef byte

#include "InvertedIndex.h"
#include "ThreadPool.h"
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "FileManager.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

class HTTPServer {
private:
    int port;
    InvertedIndex& index;
    ThreadPool thread_pool;
    FileManager file_manager;

    void handle_client(SOCKET client_socket);
    std::string process_request(const std::string& request);
    std::string generate_html_page();
    std::string generate_search_results(const std::string& word);
    std::string generate_admin_login_page();
    std::string process_admin_login(const std::string& request);
    std::string generate_admin_panel();
    std::string process_admin_action(const std::string& request);


public:
    HTTPServer(int port, InvertedIndex& index, size_t thread_count = 4);
    void start();
};

#endif
