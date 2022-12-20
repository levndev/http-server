#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <string>
#include <regex>
#include <fstream>
#include <sys/stat.h>
#include <thread>

std::string ReadFile(std::string path, bool * success) {
    std::fstream file;
    file.open(path, std::fstream::in);
    if (file.is_open())
        *success = true;
    else
        *success = false;
    std::stringstream ssBuffer;
    ssBuffer << file.rdbuf();
    std::string content = ssBuffer.str();
    return content;
}
bool test = false;
std::string working_directory;
void Send200(int sfd, std::string msg) {
    std::string response;
    response.append("HTTP/1.0 200 OK\r\n");
    response.append("Content-length: ");
    response.append(std::to_string(msg.size()));
    response.append("\r\n");
    response.append("Content-Type: text/html\r\n\r\n");
    response.append(msg);
    response.append("\r\n\r\n");
    send(sfd, response.c_str(), response.size(), 0);
    close(sfd);
}
void Send404(int sfd) {
    std::string response;
    response.append("HTTP/1.0 404 NOT FOUND\r\n") ;
    response.append("Content-Length: 0\r\n") ;
    response.append("Content-Type: text/html\r\n\r\n");
    send(sfd, response.c_str(), response.size(), 0);
    close(sfd);
}
void WorkConnection(int sfd) {
    int flags = 0;
    try {
        std::vector<char> buffer(10000);
        std::string msg;
        int received;
        do
        {
            received = recv(sfd, &buffer[0], buffer.size(), 0);
            if(received != -1)
                msg.append(buffer.cbegin(), buffer.cend());
        } while(received == buffer.size());
        size_t left = msg.find_first_of(' ') + 1;
        size_t right = msg.find_first_of(" ?", left);
        std::string path = msg.substr(left, right - left);
        if (test) {
            Send404(sfd);
            return;
        }
        int file_length;
        bool success = false;
        std::string file_text = ReadFile(working_directory + path, &success);
        if (success) {
            Send200(sfd, file_text);
        }
        else {
            Send404(sfd);
        }
    } catch(const std::exception& ex) {
        Send200(sfd, ex.what());
    }
}

int main(int argc, char ** argv) {
    int opt;
    std::string host = "0.0.0.0";
    std::string port = "12345";
    std::string directory = "./"; 
    while((opt = getopt(argc, argv, "h:p:d:t")) != -1){
        switch(opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'd':
                working_directory = optarg;
                break;
            case 't':
                test = true;
                break;
        }
    }
    daemon(1, 0);
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int p = atoi(port.c_str());
    in_addr h;
    inet_pton(AF_INET, host.c_str(), &h);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(p);
    addr.sin_addr = h;
    socklen_t socklen = sizeof(sockaddr_in);
    if (bind(server_fd, (struct sockaddr*)(&addr), sizeof(addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    bool running = true;
    while(running) {
        int connection = accept(server_fd, (struct sockaddr*)(&addr), &socklen);
        if (connection != -1) {
            std::thread worker(WorkConnection, connection);
            worker.detach();
        }
    }
    return 0;
}