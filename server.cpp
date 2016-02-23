#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define BUF_LEN 1024
#define PORT "8080"


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0)
        ;
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
    char buffer[BUF_LEN];
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
        
        if(fork()) { /* parent process */
            continue;
        } else { /* child process */
            while(true) {
                int res = recv(slave_socket, buffer, BUF_LEN, 0);
                if(res == -1) {
                    std::cerr << "recv() error" << std::endl;
                    close(slave_socket);
                    return 1;
                } else if(res == 0) {
                    std::cout << "Connection closed by client " << s << std::endl;
                    break;
                } else {
                    buffer[res] = '\0';
                    std::cout << "getting: " <<  buffer << std::endl;
                }
            }
            close(slave_socket);
        }
    }

    return 0;
}


