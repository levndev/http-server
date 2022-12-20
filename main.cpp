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
    std::fstream input;
    input.open(path, std::fstream::in | std::fstream::out);
    if (input.is_open())
        *success = true;
    else
        *success = false;
    std::stringstream ssBuffer;
    ssBuffer << input.rdbuf();
    std::string content = ssBuffer.str();
    return content;
}
bool test = false;
std::string working_directory;
void SendMsg(int sfd, std::string msg) {
    std::string response;
    response.append("HTTP/1.0 200 OK\r\n");
    response.append("Content-length: ");
    response.append(std::to_string(msg.size()));
    response.append("\r\n");
    response.append("Content-Type: text/html\r\n\r\n");
    response.append(msg);
    response.append("\r\n\r\n");
    //std::cout << response << std::endl;
    char *cstr = new char[response.length() + 1];
    strcpy(cstr, response.c_str());
    // do stuff
    send(sfd, cstr, response.size(), 0);

    delete [] cstr;
    //send(sfd, response.c_str(), response.size(), 0);
    close(sfd);
}
void Send404(int sfd) {
    std::string response;
    response.append("HTTP/1.0 404 NOT FOUND\r\n") ;
    response.append("Content-Length: 0\r\n") ;
    response.append("Content-Type: text/html\r\n\r\n");
    //response.append("404");
    //std::cout << response << std::endl;
    send(sfd, response.c_str(), response.size(), 0);
    close(sfd);
}
void * WorkConnection(void * data) {
    int sfd = *(int*)data;
    int flags = 0;
    try {
        std::regex rgx(R"((\w+)\s+(\/[^\s\?]*)\S*\s+HTTP\/[\d.]+)");
        std::vector<char> buffer(10000);
        std::string rcv;
        int received;
        do
        {
            received = recv(sfd, &buffer[0], buffer.size(), 0);
            if(received == -1)
            {
                //std::cout << "0 bytes received (error)" << std::endl;
            }
            else
            {
                rcv.append(buffer.cbegin(), buffer.cend());
            }

        } while(received == buffer.size());
        std::smatch match;
        if (std::regex_search(rcv, match, rgx)) {
            if (test) {
                Send404(sfd);
                return NULL;
            }
            std::string type = match[1];
            std::string resource = match[2];
            if (type == "GET") {
                //std::cout << type << " - " << resource << std::endl;
                int file_length;
                bool success = false;
                std::string file_text = ReadFile(working_directory + resource, &success);
                if (success) {
                    std::string response;
                    response.append("HTTP/1.0 200 OK\r\n");
                    response.append("Content-length: ");
                    response.append(std::to_string(file_text.size()));
                    response.append("\r\n");
                    response.append("Content-Type: text/html\r\n\r\n");
                    response.append(file_text);
                    response.append("\r\n\r\n");
                    //std::cout << response << std::endl;
                    send(sfd, response.c_str(), response.size(), 0);
                }
                else {
                    std::string response;
                    response.append("HTTP/1.0 404 NOT FOUND\r\n") ;
                    response.append("Content-Length: 0\r\n") ;
                    response.append("Content-Type: text/html\r\n\r\n");
                    //response.append("404");
                    //std::cout << response << std::endl;
                    send(sfd, response.c_str(), response.size(), 0);
                }
            }
        }
    } catch(std::exception& ex) {
        SendMsg(sfd, ex.what());
    }
    close(sfd);
    return NULL;
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
                //std::cout << "host: " << host << std::endl;
                break;
            case 'p':
                port = optarg;
                //std::cout << "port: " << port << std::endl;
                break;
            case 'd':
                working_directory = optarg;
                //std::cout << "directory: " << directory << std::endl;
                break;
            case 't':
                test = true;
                //std::cout << "directory: " << directory << std::endl;
                break;
        }
    }
    daemon(1, 0);
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // int optVal = 1;
    // if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optVal, sizeof(optVal))) {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }
    int p = atoi(port.c_str());
    //char host[sizeof(struct in_addr)];
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
            //std::thread worker(WorkConnection, connection, directory, test);
            //worker.detach();
            pthread_t slavethread;
            int pthreadCheck = pthread_create(&slavethread, NULL, WorkConnection, &connection);
            if(pthreadCheck != 0)
            {
                //std::cout << "Something wrong with pthread creation" << std::endl;
            }
            int pthrDet = pthread_detach(slavethread);
            if(pthrDet != 0)
            {
                //std::cout << "Something wrong with pthread detaching" << std::endl;
            }
        }
    }
    // valread = read(new_socket, buffer, 1024);
    // printf("%s\n", buffer);
    // send(new_socket, hello, strlen(hello), 0);
    // printf("Hello message sent\n");
    //shutdown(server_fd, SHUT_RDWR);
    return 0;
}