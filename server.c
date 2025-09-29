#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 502
#define BUF_SIZE 127

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
    char buffer[BUF_SIZE] = {0};

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

    _ssize_t valread;
    while(1){
        struct ModbusFrame packet;

        // for modbus transactions, protocol ID is always 0
        packet.prot_id = 0;
        // fixed unit ID for simplicity sake
        packet.unit_id = 1;

        // Memory allocation for data section
        packet.data = (char *)malloc(119 * sizeof(char));

        packet.transac_id = 0x1234;
        packet.length = 1;
        packet.func_code = 3;
        packet.length = 8;

        unsigned char *transac_id_ptr = (unsigned char *)&packet.transac_id;
        unsigned char *prot_id_ptr = (unsigned char *)&packet.prot_id;
        unsigned char *length_ptr = (unsigned char *)&packet.length;

        // Accept connections
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Connection error");
        }
        else {
            printf("Connection accepted\n");   
            char buff_sent[BUF_SIZE] = {transac_id_ptr[LSB],
                                        transac_id_ptr[MSB],
                                        prot_id_ptr[LSB],
                                        prot_id_ptr[MSB],
                                        length_ptr[LSB],
                                        length_ptr[MSB],
                                        packet.unit_id, 
                                        packet.func_code};
            for (int i = 0; i < 8; i++) {
                printf("Byte %d: 0x%02X\n", i, (unsigned char)buff_sent[i]);
            }
            //while((valread = read(new_socket, buffer, BUF_SIZE)) > 0) {
                
                send(new_socket, buff_sent, sizeof(buff_sent), 0);
                //memset(buff_sent, 0, sizeof(buff_sent));
            //}
        }
        free(packet.data);
    }

    return 0;
}