#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define PORT "8888"
#define FILE_TRANSFER_PORT "8889"
#define BUF_SIZE 1024

int GetNewConnection(const char *nodename, const char *port) {
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(nodename, port, &hints, &servinfo) != 0) {
        std::cerr << "getaddrinfo() error" << std::endl;
        return -1;
    }
    int master_socket = 0;
    int waiting_count = 20;
    bool good_conn_flag = false;
    for(int i = 0; i < waiting_count; ++i) {
        master_socket = socket(servinfo->ai_family, 
                                   servinfo->ai_socktype,
                                   servinfo->ai_protocol);
        if(master_socket == -1) {
            std::cerr << "socket() error" << std::endl;
            return -1;
        }

        if(connect(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
            close(master_socket);
            continue;
        }
        good_conn_flag = true;
        break;
    }
    if(!good_conn_flag) {
        std::cerr << "connect() error" << std::endl;
        return -1;
    }

    freeaddrinfo(servinfo);
    return master_socket;
}

void ReceiveFile(const char *nodename, int cmd_socket_fd, int file_fd)
{
    int file_socket_fd = GetNewConnection(nodename, FILE_TRANSFER_PORT);
    if(file_socket_fd == -1) {
        char bad_answer[] = "bad";
        send(cmd_socket_fd, bad_answer, strlen(bad_answer), MSG_NOSIGNAL);
        return;
    }


    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);
    int bytes_read = recv(cmd_socket_fd, buffer, BUF_SIZE, 0);
    char good_answer[] = "Server create new connection";
    
    if(!strncmp(good_answer, buffer, bytes_read)) {
        char msg[] = "Client listen new connection";
        send(cmd_socket_fd, msg, strlen(msg) + 1, MSG_NOSIGNAL);
        std::cout << "..file transfer starts.." << std::endl;

        //file transfer
        char buffer[BUF_SIZE];
        while(true) {
            memset(buffer, 0, BUF_SIZE);
            int bytes_read = recv(file_socket_fd, buffer, BUF_SIZE, 0);
            if(bytes_read == 0) {
                std::cout << "..file transfer ends.." << std::endl;
                close(file_socket_fd);
                break;
            } else 
                write(file_fd, buffer, bytes_read);
        }
    } else {
        std::cout << "Server error: " << buffer << std::endl;
        return;
    }
}

void TryReceiveFile(const char *nodename, int socket_fd, std::string &filename)
{
    int file_fd = 0;
    std::string msg("");
    if( (file_fd = creat(filename.c_str(), 0666)) == -1) {
        msg = "Client can't open file\n";
        std::cerr << msg;
        send(socket_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
        return;
    }
    msg = "Ready to get file";
    send(socket_fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
    ReceiveFile(nodename, socket_fd, file_fd);
    close(file_fd);
}

std::string return_filename(char * buffer)
{
    std::vector<std::string> str;
    std::istringstream iss(buffer);
    std::string tmp;
    char split_char = ' ';
    while(std::getline(iss, tmp, split_char)) {
        str.push_back(tmp);
    }
    return str[3];
}


int main(int argc, char ** argv)
{
    if( argc != 2) {
        std::cerr << "Invalid usage. Right way: client {ip, hostname}.."
                  << std::endl;
        return 1;
    }

    int master_socket = GetNewConnection(argv[1], PORT);
    if(master_socket == -1)
        return -1;
    char buffer[BUF_SIZE];
    int res = recv(master_socket, buffer, BUF_SIZE - 1, 0);
    if(res == -1) {
        std::cerr << "recv() error" << std::endl;
        return 1;
    } else {
        //buffer[res] = '\0';
        std::cout << buffer;
    }

    std::regex re("^Starting\\ file\\ transfer\\ \\w+");
    while(true) {
        std::string response("");
        std::getline(std::cin, response);
        send(master_socket, response.c_str(), response.size() + 1, MSG_NOSIGNAL);
        if(response == "exit")
            break;
        char buffer[BUF_SIZE];
        int res = recv(master_socket, buffer, BUF_SIZE - 1, 0);
        if(res == -1) {
            std::cerr << "recv() error" << std::endl;
            return 1;
        } else {
            std::string new_buffer(buffer);
            if( std::regex_search(new_buffer, re) )  {
                std::string filename = return_filename(buffer);
                TryReceiveFile(argv[1], master_socket, filename);
            } else {
                std::cout << buffer;
            }
        }
    }    

    close(master_socket);
    return 0;
}
