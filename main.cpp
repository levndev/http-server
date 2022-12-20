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

char * ReadFile(std::string path, int * length) {
    //std::cout << path << std::endl;
    std::ifstream t;
    t.open(path);      // open input file
    if (!t.good()) {
        *length = -1;
        return NULL;
    }
    t.seekg(0, std::ios::end);    // go to the end
    *length = t.tellg();           // report location (this is the length)
    if (*length == -1) {
        return NULL;
    }
    t.seekg(0, std::ios::beg);    // go back to the beginning
    char * buffer = new char[*length];    // allocate memory for a buffer of appropriate dimension
    t.read(buffer, *length);       // read the whole file into the buffer
    t.close();
    return buffer;
}
bool test = false;
std::string working_directory;
void * WorkConnection(void * data) {
    int sfd = *(int*)data;
    ssize_t length;
    char buf[10240];
    memset(&buf, 0, sizeof(buf));
    int flags = 0;
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
        std::string type = match[1];
        std::string resource = match[2];
        if (type == "GET") {
            //std::cout << type << " - " << resource << std::endl;
            int file_length;
            char * file_text = ReadFile(working_directory + resource, &file_length);
            if (file_text != NULL) {
                std::string response;
                response.append("HTTP/1.0 200 OK\r\n");
                response.append("Content-length: ");
                response.append(std::to_string(file_length));
                response.append("\r\n");
                response.append("Content-Type: text/html\r\n\r\n");
                response.append(file_text);
                response.append("\r\n\r\n");
                //std::cout << response << std::endl;
                send(sfd, response.c_str(), response.size(), 0);
                delete file_text;
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
    sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    inet_pton(AF_INET, host.c_str(), (void*)&address.sin_addr.s_addr);
    address.sin_port = htons(stoi(port));
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    bool running = true;
    while(running) {
        int connection = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (connection != -1) {
            //std::thread worker(WorkConnection, connection, directory, test);
            //worker.detach();
            if (test) {
                std::string response;
                response.append("HTTP/1.0 404 NOT FOUND\r\n") ;
                response.append("Content-Length: 0\r\n") ;
                response.append("Content-Type: text/html\r\n\r\n");
                //response.append("404");
                //std::cout << response << std::endl;
                send(connection, response.c_str(), response.size(), 0);
                close(connection);
                continue;
            }
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