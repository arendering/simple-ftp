#include <iostream>
#include <string>
#include <regex>
#include <sstream>
#include <fstream>

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>

#define BUF_LEN 1024
#define PORT "8888"
#define FILE_TRANSFER_PORT "8889"

int GetNewConnection(const char * port)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* IPv4 family */
    hints.ai_socktype = SOCK_STREAM; /* stream socket TCP */
    hints.ai_flags = AI_PASSIVE; /* for server: receiving client with any addr */

    if(getaddrinfo(NULL, port, &hints, &servinfo) != 0) {
        std::cerr << "getaddrinfo() error" << std::endl;
        return -1;
    }

    int master_socket = socket(servinfo->ai_family,
                               servinfo->ai_socktype,
                               servinfo->ai_protocol);
    if(master_socket == -1) {
        std::cerr << "socket() error" << std::endl;
        return -1;
    }

    if(bind(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        std::cerr << "bind() error" << std::endl;
        close(master_socket);
        return -1;
    }

    freeaddrinfo(servinfo); /* ends with addrinfo */

    /* reuse port */
    int optval = 1;
    if(setsockopt(master_socket, SOL_SOCKET, 
                  SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        std::cerr << "setsockopt() error" << std::endl;
        return -1;
    }

    if(listen(master_socket, SOMAXCONN) == -1) {
        std::cerr << "listen() error" << std::endl;
        close(master_socket);
        return -1;
    }

    return master_socket;
}

void SigchldHandler(int signum)
{
    while(waitpid(-1, NULL, WNOHANG) > 0) 
        ;
}

int WhatToDo(const char *buffer)
{
    std::regex re("^download\\ \\w+");
    std::string str(buffer);
    if(str == "exit")
        return 1;
    else if(str == "list")
        return 2;
    else if(std::regex_search(str, re))
        return 3;
    else 
        return 4; 
}

std::vector<std::string> GetFilesInDirectory()
{
    std::vector<std::string> files;
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if(d) {
        while( (dir = readdir(d)) != NULL ) {
            if(dir->d_type == DT_REG) { // checking that it's a regular file
                                        // not a directory or smth else
                std::string buf(dir->d_name);
                files.push_back(buf);
            }
        }    
    } else 
       std::cerr << "opendir() error" << std::endl; 
    return files;
}

void SendListing(int fd)
{
    std::vector<std::string> files = GetFilesInDirectory();
    std::string response("");
    if(!files.empty()) {
        response = "Directory listing: ";
        for(auto &file : files) {
            response += file + ", ";
        }
        size_t i = response.size();
        size_t n = 2;
        response.erase(i-n, n);
        response.push_back('\n');
    } else
        response = "There is no files..\n";

    send(fd, response.c_str(), response.size() + 1, MSG_NOSIGNAL);
}

std::string CutFilename(char *buffer)
{
    std::vector<std::string> files;
    std::istringstream iss(buffer); // create a stream from string
    char split_ch = ' ';
    std::string tmp;
    while(std::getline(iss, tmp, split_ch)) {
        files.push_back(tmp);
    }
    return files[1];
}

bool isFileExist(std::string &filename)
{
    std::vector<std::string> files = GetFilesInDirectory();
    for(auto &file : files) {
        if(file == filename) 
            return true;
    }
    return false;
}


void SendFile(int cmd_socket_fd, int file_fd)
{
    std::string msg("");
    int master_socket = GetNewConnection(FILE_TRANSFER_PORT);
    int file_socket_fd = 0;
    if(master_socket == -1) {
        msg = "Unable to create connection to receive file, socket() error..\n";
        send(cmd_socket_fd, msg.c_str(), msg.size() + 1, MSG_NOSIGNAL);
        return;
    } else {
        struct sockaddr_in client_addr;
        socklen_t cli_len= sizeof(client_addr);
        file_socket_fd = accept(master_socket, 
                              (struct sockaddr *) &client_addr,
                              &cli_len);
        if(file_socket_fd == -1) {
            std::cerr << "accept() error" << std::endl;
            close(master_socket);
            msg = "Unable to create connection to receive file, accept() error\n";
            send(cmd_socket_fd, msg.c_str(), msg.size() + 1, MSG_NOSIGNAL);
            return;
        } else {
            msg = "Server create new connection";
            send(cmd_socket_fd, msg.c_str(), msg.size() + 1, MSG_NOSIGNAL);

            char buffer[BUF_LEN];
            memset(buffer, 0, BUF_LEN);
            int bytes_read = recv(cmd_socket_fd, buffer, BUF_LEN, 0);
            char good_answer[] = "Client listen new connection";
            if(!strncmp(buffer, good_answer, bytes_read)) {
                struct stat stat_buf;
                fstat(file_fd, &stat_buf);
                off_t offset = 0;
                sendfile(file_socket_fd, file_fd, &offset, stat_buf.st_size);
            }
            shutdown(file_socket_fd, SHUT_RDWR);
            close(file_socket_fd);
            close(master_socket);

        }
    }
}


void TrySendFile(std::string &filename, int socket_fd)
{
    std::string msg("");
    int file_fd = 0;
    if( !isFileExist(filename) ) {
        msg = "File \"" + filename + "\" isn't exist\n";
        send(socket_fd, msg.c_str(), msg.size() + 1, MSG_NOSIGNAL);
        return;
    }
    else if ( (file_fd = open(filename.c_str(), O_RDONLY)) == -1) {
        msg = "Oops, can't open a file\n";
        send(socket_fd, msg.c_str(), msg.size() + 1, MSG_NOSIGNAL);
        return;
    }

    msg = "Starting file transfer " + filename;
    send(socket_fd, msg.c_str(), msg.size() + 1, MSG_NOSIGNAL);
    char buffer[BUF_LEN];
    memset(buffer, 0, BUF_LEN);
    int read_bytes = recv(socket_fd, buffer, BUF_LEN - 1, 0);
    if( !strncmp(buffer, "Ready to get file", read_bytes)) {
        SendFile(socket_fd, file_fd); 
    }
    close(file_fd);
}


int main(int argc, char ** argv)
{
    int master_socket = GetNewConnection(PORT); 
    
    struct sigaction sigact;
    sigact.sa_handler = SigchldHandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sigact, NULL) == -1) {
        std::cerr << "sigaction() error" << std::endl;
        return 1;
    }
    
    std::cout << "Server waits for connections..\n";
    int slave_socket = 0;
    struct sockaddr_in client_addr;
    socklen_t cli_len = sizeof(client_addr);

    while(true) {

        slave_socket = accept(master_socket, 
                              (struct sockaddr *) &client_addr,
                              &cli_len);
        // slow syscalls may return EINTR if it was interrupted by signal
        if(errno == EINTR) 
            continue;
        if(slave_socket == -1) {
            std::cerr << "accept() error" << std::endl;
            close(master_socket);
            return 1;
        } 
        char s[INET_ADDRSTRLEN];
        inet_ntop(client_addr.sin_family,
                  &client_addr.sin_addr,
                  s, sizeof(s));
        std::cout << "Got connection from: " << s << std::endl;
        bool flag = true;

       if(fork()) { // parent process
            continue;
        } else { // child process 
            while(true) {
                if(flag) {
                    std::string how_to_use = 
                        "Available commands: list, download <filename>\n";
                    send(slave_socket, how_to_use.c_str(), 
                            how_to_use.size() + 1, MSG_NOSIGNAL);
                    flag = false;
                }
                char buffer[BUF_LEN];
                int res = recv(slave_socket, buffer, BUF_LEN, 0);
                if(res == -1) {
                    std::cerr << "recv() error" << std::endl;
                    close(slave_socket);
                    return 1;
                } else if(res == 0) {
                    break;
                } else {
                    //buffer[res] = '\0';
                    int val = WhatToDo(buffer);
                    switch (val) {
                        case 1:
                            goto end_loop;

                        case 2:
                            SendListing(slave_socket);
                            break;
                            
                        case 3: {
                            std::string filename = CutFilename(buffer);
                            TrySendFile(filename, slave_socket);
                        } break;

                        case 4: {
                            std::string response = "Unknown command..\n";
                            send(slave_socket, response.c_str(),
                                   response.size() + 1, MSG_NOSIGNAL);
                        } break; 
                    }
                }
            }
            end_loop: ;
            std::cout << "Connection closed by client " << s << std::endl;
            shutdown(slave_socket, SHUT_RDWR);
            close(slave_socket);
        }
    }

    return 0;
}


