#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define BUF_LEN 1024
#define PORT "8888"
#define SERV "localhost"

int main(int argc, char ** argv)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* IPv4 family */
    hints.ai_socktype = SOCK_STREAM; /* stream socket TCP */
    hints.ai_flags = AI_PASSIVE; /* for server: receiving client with any addr */

    if(getaddrinfo(SERV, PORT, &hints, &servinfo) != 0) {
        perror("getaddrinfo() error");
        return 1;
    }

    int master_socket = socket(servinfo->ai_family,
                               servinfo->ai_socktype,
                               servinfo->ai_protocol);
    if(master_socket == -1) {
        perror("socket() error");
        return 1;
    }

    if(bind(master_socket, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("bind() error");
        close(master_socket);
        return 1;
    }

    freeaddrinfo(servinfo); /* ends with addrinfo */

    /* reuse port */
    int optval = 1;
    if(setsockopt(master_socket, SOL_SOCKET, 
                  SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt() error");
        return 1;
    }

    if(listen(master_socket, SOMAXCONN) == -1) {
        perror("listen() error");
        close(master_socket);
        return 1;
    }

    char buffer[BUF_LEN];
    struct sockaddr_in client_addr;
    socklen_t sin_size = sizeof(client_addr);
    int slave_socket = accept(master_socket, 
                              (struct sockaddr *) &client_addr,
                              &sin_size);
    if(slave_socket == -1) {
        perror("accept() error");
        close(master_socket);
        return 1;
    } 
    char s[INET_ADDRSTRLEN];
    inet_ntop(client_addr.sin_family,
              &client_addr.sin_addr,
              s, sizeof(s));
    printf("Got connection from: %s\n", s);

    while(1) {
        
        int res = recv(slave_socket, buffer, BUF_LEN, 0);
        if(res == -1) {
            perror("recv() error");
            close(slave_socket);
            return 1;
        } else if(res == 0) {
            printf("Connection closed by client..\n");
            break;
        } else {
            buffer[res] = '\0';
            printf("getting: %s", buffer);
        }
    }

    if(shutdown(slave_socket, SHUT_RDWR) == -1)
        perror("shutdown() error");
    close(slave_socket);
    return 0;
}


