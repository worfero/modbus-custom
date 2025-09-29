#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#define MAX 127
#define PORT 502
#define SA struct sockaddr

int main()
{
    int new_socket;
    struct sockaddr_in servaddr;

    // socket create and verification
    new_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (new_socket == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(new_socket, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    char signal[1] = {0};
    write(new_socket, signal, sizeof(signal));

    char buffer_rec[MAX] = {0};
    recv(new_socket, buffer_rec, sizeof(buffer_rec) - 1, 0);
    printf("0x");
    for(int i = 0; i < 8; i++){
        printf("%02X ", (unsigned char)buffer_rec[i]);
    }
    printf("\n");

    // close the socket
    close(new_socket);
}