#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 502
#define BUF_SIZE 128

#define MSB 1
#define LSB 0

struct ModbusFrame {
    // MBAP Header
    short transac_id;
    short prot_id;
    short length;
    char unit_id;
    // Application layer
    char func_code;
    char *data;
};

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket fd
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed!");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Set socket options failed");
        exit(EXIT_FAILURE);
    }

    // Setup address (IPv4)
    address.sin_family = AF_INET;

    address.sin_addr.s_addr = INADDR_ANY;

    address.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if(listen(server_fd, 3) < 0) {
        perror("listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while(1){
        struct ModbusFrame packet;

        // for modbus transactions, protocol ID is always 0
        packet.prot_id = 0;
        // fixed unit ID for simplicity sake
        packet.unit_id = 1;

        // Memory allocation for data section
        packet.data = (char *)malloc(119 * sizeof(char));
        
        packet.transac_id = 0;
        packet.func_code = 3;
        packet.length = 5;

        char *transac_id_ptr = (char *)&packet.transac_id;
        char *prot_id_ptr = (char *)&packet.prot_id;
        char *length_ptr = (char *)&packet.length;

        // Accept connections
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Connection error");
        }
        else {
            printf("Connection accepted\n");   
            char *buff_recv = (char *)malloc(BUF_SIZE * sizeof(char));
            char *buff_sent = (char *)malloc(BUF_SIZE * sizeof(char));
            buff_sent[0] = transac_id_ptr[MSB];
            buff_sent[1] = transac_id_ptr[LSB];
            buff_sent[2] = prot_id_ptr[MSB];
            buff_sent[3] = prot_id_ptr[LSB];
            buff_sent[4] = length_ptr[MSB];
            buff_sent[5] = length_ptr[LSB];
            buff_sent[6] = packet.unit_id;
            buff_sent[7] = packet.func_code;
            buff_sent[8] = 0x02;
            buff_sent[9] = 0x00;
            buff_sent[10] = 0x25;
            _ssize_t bytes_recv;
            if((bytes_recv = read(new_socket, buff_recv, BUF_SIZE)) > 0) {
                printf("Client message: 0x");
                for(int i = 0; i < bytes_recv; i++){
                    printf("%02X ", (char)buff_recv[i]);
                }
                printf("\n");
                printf("Server response: 0x");
                for(int i = 0; i <= 10; i++){
                    printf("%02X ", (char)buff_sent[i]);
                }
                printf("\n");
                send(new_socket, buff_sent, 11, 0);
                packet.transac_id++;
            }
        }
        free(packet.data);
    }

    return 0;
}