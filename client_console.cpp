#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <regex>

#pragma comment(lib, "ws2_32.lib")

void send_request(const std::string& server_address, int port, const std::string& request, bool parse_response = false, bool admin_response = false) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, server_address.c_str(), &server.sin_addr) <= 0) {
        std::cerr << "Invalid server address" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    send(client_socket, request.c_str(), request.size(), 0);

    char buffer[4096] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string response(buffer);

        response.erase(response.find_last_not_of(" \n\r\t") + 1);

        if (parse_response) {
            std::regex results_regex("Document: (.+), Positions: (.+)");
            std::smatch match;
            auto start = response.cbegin();
            auto end = response.cend();
            while (std::regex_search(start, end, match, results_regex)) {
                std::cout << "Document: " << match[1] << ", Positions: " << match[2] << "\n";
                start = match.suffix().first;
            }
        } else if (admin_response) {
            if (response.find("200 OK") != std::string::npos) {
                std::cout << std::endl << "Action completed successfully." << std::flush;
            } else {
                std::cerr << "Action failed." << std::flush;
            }
        } else {
            std::cout << "Response from server:\n" << response << std::flush;
        }
    } else {
        std::cerr << "Failed to receive data" << std::endl;
    }

    closesocket(client_socket);
    WSACleanup();
}

bool authenticate_admin(const std::string& server_address, int port) {
    std::cout << "Enter admin password: " << std::flush;
    std::string password;
    std::cin >> password;

    std::string request = "POST /admin/login HTTP/1.1\r\n"
                          "Host: " + server_address + "\r\n"
                                                      "Content-Type: application/x-www-form-urlencoded\r\n"
                                                      "Content-Length: " + std::to_string(9 + password.size()) + "\r\n"
                                                                                                                 "Connection: close\r\n\r\n"
                                                                                                                 "password=" + password;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return false;
    }

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, server_address.c_str(), &server.sin_addr) <= 0) {
        std::cerr << "Invalid server address" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return false;
    }

    if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return false;
    }

    send(client_socket, request.c_str(), request.size(), 0);

    char buffer[4096] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    bool authenticated = false;
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string response(buffer);
        if (response.find("200 OK") != std::string::npos) {
            authenticated = true;
        }
    } else {
        std::cerr << "Failed to receive authentication response" << std::endl;
    }

    closesocket(client_socket);
    WSACleanup();

    if (!authenticated) {
        std::cerr << "Invalid password" << std::endl;
    }

    return authenticated;
}

void search_word(const std::string& server_address, int port) {
    std::cout << "Enter the word to search: " << std::flush;
    std::string word;
    std::cin >> word;

    std::string request = "GET /query?word=" + word + " HTTP/1.1\r\n"
                                                      "Host: " + server_address + "\r\n"
                                                                                  "Connection: close\r\n\r\n";
    send_request(server_address, port, request, true);
}

void admin_action(const std::string& server_address, int port) {
    if (!authenticate_admin(server_address, port)) {
        return;
    }

    std::cout << "Choose action (add/remove): " << std::flush;
    std::string action;
    std::cin >> action;

    if (action != "add" && action != "remove") {
        std::cerr << "Invalid action" << std::endl;
        return;
    }

    std::cout << "Enter file name: " << std::flush;
    std::string file_name;
    std::cin >> file_name;

    std::string request;
    if (action == "add") {
        std::cout << "Enter file content: " << std::flush;
        std::cin.ignore();
        std::string content;
        std::getline(std::cin, content);

        request = "POST /admin/action HTTP/1.1\r\n"
                  "Host: " + server_address + "\r\n"
                                              "Content-Type: application/x-www-form-urlencoded\r\n"
                                              "Content-Length: " + std::to_string(13 + file_name.size() + content.size()) + "\r\n"
                                                                                                                            "Connection: close\r\n\r\n"
                                                                                                                            "action=add&file=" + file_name + "&content=" + content;
    } else if (action == "remove") {
        request = "POST /admin/action HTTP/1.1\r\n"
                  "Host: " + server_address + "\r\n"
                                              "Content-Type: application/x-www-form-urlencoded\r\n"
                                              "Content-Length: " + std::to_string(13 + file_name.size()) + "\r\n"
                                                                                                           "Connection: close\r\n\r\n"
                                                                                                           "action=remove&file=" + file_name;
    }

    send_request(server_address, port, request, false, true);
}

int main() {
    std::string server_address = "127.0.0.1";
    int port = 8082;

    while (true) {
        std::cout << "\nMenu:\n";
        std::cout << "1. Search word\n";
        std::cout << "2. Admin action (add/remove file)\n";
        std::cout << "3. Exit\n";
        std::cout << "Choose an option: " << std::flush;

        int choice;
        std::cin >> choice;

        if (std::cin.fail() || choice < 1 || choice > 3) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "Invalid option, please try again." << std::endl;
            continue;
        }

        switch (choice) {
            case 1:
                search_word(server_address, port);
                break;
            case 2:
                admin_action(server_address, port);
                break;
            case 3:
                return 0;
        }
    }
}
