#include <stdexcept>
#include <zlib.h>
#include <arpa/inet.h>
#include <sstream>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>
#include <fstream>
#include <vector>

#define MOD_GZIP_ZLIB_WINDOWSIZE 15
#define MOD_GZIP_ZLIB_CFACTOR 9 
#define MOD_GZIP_ZLIB_BSIZE 8096

std::vector<std::string> validEncodings = {"gzip", };

int sendMessage(int clientSocket, const char* response, int size) {
    if (send(clientSocket, response, size, 0) == -1) {
        std::cerr << "Failed to send message";
        return 1;
    }

    return 0;
}

std::string getPath(char* buf, int size) {
    int i {}, j {};

    for (i=0;i<size;i++) {
        if (buf[i] == ' ')
            break;
    }

    i++;

    for (j=i+1;j<size;j++) {
        if (buf[j] == ' ')
            break;
    }

    std::string path = "";
    for (;i<j;i++) {
        path.push_back(buf[i]);
    }

    return path;
}

std::map<std::string, std::string> getHeaders(char* buf, int size) {
    int i {}, j {}, k {};
    std::map<std::string, std::string> headers;

    for (i=0;i<size && buf[i] != '\n';i++);
    i++;

    while (true) {
        std::string key, value;
        for (j=i;j<size && buf[j] != ':';j++) {
            key.push_back(buf[j]);
        }
        for (k=j+2;k<size && buf[k] != '\r';k++) {
            value.push_back(buf[k]);
        }

        // std::cout << '\'' << key << ":"<< value << '\'' << '\n';

        headers[key] = value;

        i = k + 2;

        if (buf[i] == '\r')
            break;
    }

    return headers;
}

std::string getMethod(char* buf, int size) {
    int i {};
    std::string method = "";

    for (i=0;i<size && buf[i] != ' ';i++) {
        method.push_back(buf[i]);
    }

    std::cout << "Method: " << '\'' << method << '\'' << '\n';
    return method;
}

std::string getBody(char* buf, int size, int content_length) {
    int i {};
    
    for (i=0;i<size;i++) {
        if (buf[i] == '\n' && buf[i+1] == '\r') {
            i += 3;
            break;
        }
    }

    std::string body = "";

    for (int k=0;k<content_length;k++) {
        body.push_back(buf[i+k]);
    }

    return body;
}

std::string validEncoding(std::map<std::string, std::string> headers) {
    if (headers.find("Accept-Encoding") == headers.end()) {
        return "";
    }

    std::vector<std::string> encodings;
    std::string accept_encodings = headers["Accept-Encoding"];
    int i {}, j {};

    while (i < accept_encodings.size()) {
        for (j=i+1;j<accept_encodings.size() && accept_encodings[j] != ',';j++);

        encodings.push_back(accept_encodings.substr(i, j-i));

        for (i=j+1;i<accept_encodings.size() && accept_encodings[i] == ' ';i++);
    }

    for (auto p: accept_encodings) {
        std::cout << "Accept encodings: \'" << p << "\'\n";
    }

    for (auto q: encodings) {
        for (auto p: validEncodings) {
            if (p == q)
                return p;
        }
    }

    return "";
}

std::string gzipCompress(std::string body, int compressionLevel = Z_BEST_COMPRESSION) {
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, 
                     compressionLevel,
                     Z_DEFLATED,
                     MOD_GZIP_ZLIB_WINDOWSIZE + 16, 
                     MOD_GZIP_ZLIB_CFACTOR,
                     Z_DEFAULT_STRATEGY) != Z_OK
    ) {
        throw(std::runtime_error("deflateInit2 failed while compressing."));
    }

    zs.next_in = (Bytef*)body.data();
    zs.avail_in = body.size();           // set the z_stream's input

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // retrieve the compressed bytes blockwise
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (outstring.size() < zs.total_out) {
            // append the block to the output string
            outstring.append(outbuffer,
                             zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

int main(int argc, char **argv) {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << argc << '\n';
    std::cout << argv[1] << '\n';
    std::cout << argv[2] << '\n';


    // You can use print statements as follows for debugging, they'll be visible
    // when running tests.
    std::cout << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage
    //
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
        0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
        0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";

    while (true) {
        int clientSocket = accept(server_fd, (struct sockaddr *)&client_addr,
               (socklen_t *)&client_addr_len);
        std::cout << "Client connected\n";
        
        char buf[1024] = {0};
        ssize_t msg_size = recv(clientSocket, &buf, (size_t)1024, 0);

        std::string path = getPath(buf, 1024);
        std::cout << "Path: \'" << path << "\'"<< '\n';

        std::map<std::string, std::string> headers = getHeaders(buf, 1024);

        std::string method = getMethod(buf, 1024);

        if (path == "/") {
            if (sendMessage(clientSocket, "HTTP/1.1 200 OK\r\n\r\n", 19) == 1) {
                return 1;
            }
        }
        else if (path.substr(0, 6) == "/echo/") {
            std::string response_body = path.substr(6);
            std::string response;
            std::cout << "Before finding encoding" << '\n';
            std::string encoding = validEncoding(headers);
            
            std::cout << "Encoding: " << encoding << '\n';
            if (encoding != "") {
                if (encoding == "gzip") {
                    response_body = gzipCompress(response_body);
                }
                response = "HTTP/1.1 200 OK\r\nContent-Encoding: "+encoding+"\r\nContent-Type: text/plain\r\nContent-Length: "+std::to_string(response_body.length())+"\r\n\r\n"+response_body;
            }
            else {
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "+std::to_string(response_body.length())+"\r\n\r\n"+response_body;
            }
            const char* response_c_str = response.c_str();

            if (sendMessage(clientSocket, response_c_str, response.length()) == 1) {
                return 1;
            }
        }
        else if (path.substr(0, 11) == "/user-agent") {
            std::string response_body = headers["User-Agent"];
            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "+std::to_string(response_body.length())+"\r\n\r\n"+response_body;
            const char* response_c_str = response.c_str();

            if (sendMessage(clientSocket, response_c_str, response.length()) == 1) {
                return 1;
            } 
        }
        else if (path.substr(0, 7) == "/files/" && method == "GET") {
            std::string file_path = argv[2];
            file_path += path.substr(7);
            std::cout << "File path: " << file_path << '\n';
            std::ifstream file(file_path);

            if (!file.good()) {
                if (sendMessage(clientSocket,"HTTP/1.1 404 Not Found\r\n\r\n", 26) == 1) {
                    return 1;
                }
            }
            else {
                std::stringstream file_buffer;
                file_buffer << file.rdbuf();
                std::string file_contents = file_buffer.str();

                std::cout << "File contents: " << file_contents << '\n';

                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "+std::to_string(file_contents.length())+"\r\n\r\n"+file_contents;
                const char* response_c_str = response.c_str();

                if (sendMessage(clientSocket, response_c_str, response.length()) == 1) {
                    return 1;
                }
            }

            file.close();
        }
        else if (path.substr(0, 7) == "/files/" && method == "POST") {
            std::string file_path = argv[2];
            file_path += path.substr(7);
            std::cout << "File path: " << file_path << '\n';
            std::string body = getBody(buf, 1024, std::stoi(headers["Content-Length"]));
            
            std::cout << "Body: " << body << '\n';

            std::ofstream file(file_path);
            file << body;

            std::string response = "HTTP/1.1 201 Created\r\n\r\n"; 
            const char* response_c_str = response.c_str();
            if (sendMessage(clientSocket, response_c_str, response.length()) == 1) {
                return 1;
            }
            file.close();
        }
        else {
            if (sendMessage(clientSocket, "HTTP/1.1 404 Not Found\r\n\r\n", 26) == 1) {
                return 1;
            }
        }
    }

    // int clientSocket = accept(server_fd, (struct sockaddr *)&client_addr,
    //        (socklen_t *)&client_addr_len);
    // std::cout << "Client connected\n";
    //
    // char buf[1024] = {0};
    // ssize_t msg_size = recv(clientSocket, &buf, (size_t)1024, 0);
    //
    // std::string path = getPath(buf, 1024);
    // std::cout << "Path: \'" << path << "\'"<< '\n';
    //
    // std::map<std::string, std::string> headers = getHeaders(buf, 1024);
    //
    // if (path == "/") {
    //     if (sendMessage(clientSocket, "HTTP/1.1 200 OK\r\n\r\n", 19) == 1) {
    //         return 1;
    //     }
    // }
    // else if (path.substr(0, 6) == "/echo/") {
    //     std::string response_body = path.substr(6);
    //     std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "+std::to_string(response_body.length())+"\r\n\r\n"+response_body;
    //     const char* response_c_str = response.c_str();
    //
    //     if (sendMessage(clientSocket, response_c_str, response.length()) == 1) {
    //         return 1;
    //     }
    // }
    // else if (path.substr(0, 11) == "/user-agent") {
    //     std::string response_body = headers["User-Agent"];
    //     std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "+std::to_string(response_body.length())+"\r\n\r\n"+response_body;
    //     const char* response_c_str = response.c_str();
    //
    //     if (sendMessage(clientSocket, response_c_str, response.length()) == 1) {
    //         return 1;
    //     } 
    // }
    // else {
    //     if (sendMessage(clientSocket, "HTTP/1.1 404 Not Found\r\n\r\n", 26) == 1) {
    //         return 1;
    //     }
    // }

    close(server_fd);

    return 0;
}

