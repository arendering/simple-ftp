#include <iostream>
#include <string>
#include <regex>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>

#define BUF_LEN 1024
#define PORT "8080"


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int what_to_do(const char *buffer)
{
    std::regex re("^download\\ \\w+");
    std::string str(buffer);
    if(str == "exit\n")
        return 1;
    else if(str == "list\n")
        return 2;
    else if(std::regex_search(str, re))
        return 3;
    else 
        return 4; 
}

std::vector<std::string> get_files_in_directory()
{
    std::vector<std::string> files;
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if(d) {
        while( (dir = readdir(d)) != NULL) {
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

void send_listing(int fd)
{
    std::vector<std::string> files = get_files_in_directory();
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

std::string cut_filename(char *buffer)
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

void send_file(std::string &filename, int fd)
{
    filename.erase(filename.size() - 1, 1); // erase \n in the end of filename
    std::vector<std::string> files = get_files_in_directory();
    bool file_exists = false;
    for(auto &file : files) {
        if(file == filename) {
            file_exists = true;
            break;
        }
    }
    if(file_exists) {
        //send file
    } else {
        std::string message = "File \"" + filename + "\" isn't exist\n";
        send(fd, message.c_str(), message.size() + 1, MSG_NOSIGNAL);
    }
}

int main(int argc, char ** argv)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* IPv4 family */
    hints.ai_socktype = SOCK_STREAM; /* stream socket TCP */
    hints.ai_flags = AI_PASSIVE; /* for server: receiving client with any addr */

    if(getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        std::cerr << "getaddrinfo() error" << std::endl;
        return 1;
    }

    int master_socket = socket(servinfo->ai_family,
                               servinfo->ai_socktype,
                               servinfo->ai_protocol);
    if(master_socket == -1) {
        std::cerr << "socket() error" << std::endl;
        return 1;
    }

    if(bind(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        std::cerr << "bind() error" << std::endl;
        close(master_socket);
        return 1;
    }

    freeaddrinfo(servinfo); /* ends with addrinfo */

    /* reuse port */
    int optval = 1;
    if(setsockopt(master_socket, SOL_SOCKET, 
                  SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        std::cerr << "setsockopt() error" << std::endl;
        return 1;
    }

    if(listen(master_socket, SOMAXCONN) == -1) {
        std::cerr << "listen() error" << std::endl;
        close(master_socket);
        return 1;
    }

    
    struct sigaction sigact;
    sigact.sa_handler = sigchld_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sigact, NULL) == -1) {
        std::cerr << "sigaction() error" << std::endl;
        return 1;
    }

    std::cout << "Server waits for connections..\n";
    int slave_socket = 0;
    struct sockaddr_in client_addr;
    socklen_t sin_size = sizeof(client_addr);

    while(true) {

        slave_socket = accept(master_socket, 
                              (struct sockaddr *) &client_addr,
                              &sin_size);
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

        if(fork()) { /* parent process */
            continue;
        } else { /* child process */
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
                    buffer[res] = '\0';
                    int val = what_to_do(buffer);
                    switch (val) {
                        case 1:
                            goto end_loop;

                        case 2:
                            send_listing(slave_socket);
                            break;
                            
                        case 3: {
                            std::string filename = cut_filename(buffer);
                            send_file(filename, slave_socket);
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


