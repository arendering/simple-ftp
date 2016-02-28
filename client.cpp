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

#define PORT "8080"
#define BUF_SIZE 1024

void receive_file(int socket_fd, int file_fd)
{
    char buffer[BUF_SIZE];
    char end_msg[] = "File transfer ends";
    while(true) {
        //int bytes_read = read(socket_fd, buffer, sizeof(buffer));
        memset(buffer, 0, BUF_SIZE);
        int bytes_read = recv(socket_fd, buffer, BUF_SIZE, 0);
        if(!strncmp(end_msg, buffer, bytes_read)) {
            std::cout << "..file transfer ends.." << std::endl;
            break;
        }
        else 
            write(file_fd, buffer, bytes_read);
    }
    
}

void try_receive_file(int socket_fd, std::string &filename)
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
    std::cout << "..file transfer starts.." << std::endl;
    receive_file(socket_fd, file_fd);
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
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(argv[1], PORT, &hints, &servinfo) != 0) {
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
    
    if(connect(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(master_socket);
        std::cerr << "connect() error" << std::endl;
        return 1;
    }

    freeaddrinfo(servinfo);
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
                try_receive_file(master_socket, filename);
            } else {
                std::cout << buffer;
            }
        }
    }    

    close(master_socket);
    return 0;
}
