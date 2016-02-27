#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define PORT "8080"
#define BUF_SIZE 1024

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
            //buffer[res] = '\0';
            std::cout << buffer;
        }

    }    

    close(master_socket);
    return 0;
}
