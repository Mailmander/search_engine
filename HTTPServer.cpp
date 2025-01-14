#include "HTTPServer.h"


HTTPServer::HTTPServer(int port, InvertedIndex& index, size_t thread_count): port(port), index(index), thread_pool(thread_count) {}

void HTTPServer::start() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("Socket creation failed");
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (::bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        closesocket(server_fd);
        WSACleanup();
        throw std::runtime_error("Bind failed");
    }

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(server_fd);
        WSACleanup();
        throw std::runtime_error("Listen failed");
    }

    std::cout << "HTTP Server listening on port " << port << "..." << std::endl;

    while (true) {
        SOCKET client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        thread_pool.enqueue([this, client_socket]() {
            handle_client(client_socket);
        });
    }

    closesocket(server_fd);
    WSACleanup();
}

void HTTPServer::handle_client(SOCKET client_socket) {
    char buffer[1024] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string request(buffer);

        std::string response = process_request(request);
        send(client_socket, response.c_str(), response.size(), 0);
    } else {
        std::cerr << "Failed to receive data from client" << std::endl;
    }

    closesocket(client_socket);
}

std::string HTTPServer::process_request(const std::string& request) {
    std::istringstream request_stream(request);
    std::string method, path;
    request_stream >> method >> path;

    if (method == "GET" && path == "/") {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Connection: close\r\n\r\n" +
               generate_html_page();
    } else if (method == "GET" && path.find("/query") == 0) {
        size_t query_pos = path.find("?");
        std::string word;
        if (query_pos != std::string::npos) {
            word = path.substr(query_pos + 6);
        }

        //std::cout << "Searching for word: " << word << std::endl;
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Connection: close\r\n\r\n" +
               generate_search_results(word);
    } else if (method == "GET" && path == "/admin") {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Connection: close\r\n\r\n" +
               generate_admin_login_page();
    } else if (method == "POST" && path == "/admin/login") {
        return process_admin_login(request);
    } else if (method == "POST" && path == "/admin/action") {
        return process_admin_action(request);
    } else {
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n\r\nPage not found";
    }
}

std::string HTTPServer::process_admin_action(const std::string& request) {
    const std::string base_directory = "./text_files/";
    size_t file_pos = request.find("file=");
    size_t action_pos = request.find("action=");

    if (file_pos == std::string::npos || action_pos == std::string::npos) {
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n\r\nMissing parameters";
    }

    std::string file_name = request.substr(file_pos + 5, request.find('&', file_pos) - (file_pos + 5));
    std::string action = request.substr(action_pos + 7, request.find('&', action_pos) - (action_pos + 7));

    std::replace(file_name.begin(), file_name.end(), '+', ' ');
    std::string full_path = base_directory + file_name;

    if (action == "add") {
        size_t content_pos = request.find("content=");
        if (content_pos == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\nMissing content for add action";
        }

        std::string content = request.substr(content_pos + 8);
        std::replace(content.begin(), content.end(), '+', ' ');

        if (!file_manager.write_file(full_path, content)) {
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\nFailed to save file";
        }

        index.add_document(full_path, content);
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n\r\nFile added successfully";
    } else if (action == "remove") {
        if (!file_manager.delete_file(full_path)) {
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\nFailed to remove file";
        }

        index.remove_document(file_name);
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n\r\nFile removed successfully";
    }

    return "HTTP/1.1 400 Bad Request\r\n"
           "Content-Type: text/plain\r\n"
           "Connection: close\r\n\r\nUnknown action";
}



std::string HTTPServer::generate_html_page() {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Search Engine</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css" rel="stylesheet">
</head>
<body>
    <header class="bg-primary text-white text-center py-3">
        <h1>Search Engine</h1>
    </header>
    <main class="container mt-5">
        <div class="row justify-content-center">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header text-center bg-primary text-white">
                        <h2>Search</h2>
                    </div>
                    <div class="card-body">
                        <form action="/query" method="get">
                            <div class="mb-3">
                                <input type="text" class="form-control" id="word" name="word" placeholder="Enter word to search">
                            </div>
                            <div class="d-flex justify-content-between">
                                <button type="submit" class="btn btn-primary">Search</button>
                                <a href="/admin" class="btn btn-secondary">Admin Panel</a>
                            </div>
                        </form>
                    </div>
                </div>
            </div>
        </div>
    </main>
</body>
</html>)";
}

std::string HTTPServer::generate_search_results(const std::string& word) {
    std::ostringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"en\">\n";
    html << "<head>\n";
    html << "    <meta charset=\"UTF-8\">\n";
    html << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "    <title>Search Results</title>\n";
    html << "    <link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css\" rel=\"stylesheet\">\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "    <header class=\"bg-primary text-white text-center py-3\">\n";
    html << "        <h1>Search Results</h1>\n";
    html << "    </header>\n";
    html << "    <main class=\"container mt-5\">\n";
    html << "        <div class=\"row justify-content-center\">\n";
    html << "            <div class=\"col-md-8\">\n";
    html << "                <div class=\"card\">\n";
    html << "                    <div class=\"card-header bg-success text-white text-center\">\n";
    html << "                        <h2>Results for: " << word << "</h2>\n";
    html << "                    </div>\n";
    html << "                    <div class=\"card-body\">\n";
    html << "                        <ul class=\"list-group\">\n";

    std::string results = index.query(word);
    if (!results.empty()) {
        html << "                            <li class=\"list-group-item\"><pre>" << results << "</pre></li>\n";
    } else {
        html << "                            <li class=\"list-group-item text-danger\">No results found</li>\n";
    }

    html << "                        </ul>\n";
    html << "                    </div>\n";
    html << "                    <div class=\"card-footer text-center\">\n";
    html << "                        <a href=\"/\" class=\"btn btn-primary\">Back to Search</a>\n";
    html << "                    </div>\n";
    html << "                </div>\n";
    html << "            </div>\n";
    html << "        </div>\n";
    html << "    </main>\n";
    html << "</body>\n";
    html << "</html>";
    return html.str();
}

std::string HTTPServer::generate_admin_login_page() {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Admin Login</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css" rel="stylesheet">
</head>
<body>
    <header class="bg-danger text-white text-center py-3">
        <h1>Admin Panel Login</h1>
    </header>
    <main class="container mt-5">
        <div class="row justify-content-center">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-body">
                        <form action="/admin/login" method="post">
                            <div class="mb-3">
                                <label for="password" class="form-label">Password</label>
                                <input type="password" class="form-control" id="password" name="password" placeholder="Enter admin password">
                            </div>
                            <button type="submit" class="btn btn-danger w-100">Login</button>
                        </form>
                    </div>
                    <div class="card-footer text-center">
                        <a href="/" class="btn btn-primary">Back to Search</a>
                    </div>
                </div>
            </div>
        </div>
    </main>
</body>
</html>)";
}

std::string HTTPServer::process_admin_login(const std::string& request) {
    size_t password_pos = request.find("password=");
    if (password_pos != std::string::npos) {
        std::string password = request.substr(password_pos + 9);
        if (password == "admin") {
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Connection: close\r\n\r\n" +
                   generate_admin_panel();
        }
    }
    return "HTTP/1.1 403 Forbidden\r\n"
           "Content-Type: text/plain\r\n"
           "Connection: close\r\n\r\nInvalid password";
}

std::string HTTPServer::generate_admin_panel() {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Admin Panel</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css" rel="stylesheet">
</head>
<body>
    <header class="bg-danger text-white text-center py-3">
        <h1>Admin Panel</h1>
    </header>
    <main class="container mt-5">
        <div class="row justify-content-center">
            <div class="col-md-8">
                <div class="card">
                    <div class="card-header bg-danger text-white">
                        <h2>Admin Actions</h2>
                    </div>
                    <div class="card-body">
                        <form action="/admin/action" method="post">
                            <div class="mb-3">
                                <label for="action" class="form-label">Action</label>
                                <select class="form-select" id="action" name="action">
                                    <option value="add">Add File</option>
                                    <option value="remove">Remove File</option>
                                </select>
                            </div>
                            <div class="mb-3">
                                <label for="file" class="form-label">File Name</label>
                                <input type="text" class="form-control" id="file" name="file" placeholder="Enter file name">
                            </div>
                            <div class="mb-3">
                                <label for="content" class="form-label">File Content (for Add only)</label>
                                <textarea class="form-control" id="content" name="content" rows="4"></textarea>
                            </div>
                            <button type="submit" class="btn btn-danger w-100">Submit</button>
                        </form>
                    </div>
                    <div class="card-footer text-center">
                        <a href="/" class="btn btn-primary">Back to Search</a>
                    </div>
                </div>
            </div>
        </div>
    </main>
</body>
</html>)";
}
