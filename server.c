#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 502
#define BUF_SIZE 128

#define LSBYTE(x) ((x << 8) & 0xFF)
#define MSBYTE(x) ((x) & 0xFF)

struct ModbusFrame {
    // MBAP Header
    short transac_id;
    short prot_id;
    short length;
    unsigned char unit_id;
    // Application layer
    unsigned char func_code;
    unsigned char *data;
};

int server_setup() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

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

    return server_fd;
}

int main() {
    int server_fd = server_setup();
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    short registers[2200] = {0};
    memset(registers, 15, sizeof(registers));
    registers[8] = 300;
    registers[9] = 200;

    while(1){
        struct ModbusFrame packet;

        // for modbus transactions, protocol ID is always 0
        packet.prot_id = 0;
        // fixed unit ID for simplicity sake
        packet.unit_id = 1;

        // Memory allocation for data section
        //packet.data = (unsigned char *)malloc(119 * sizeof(unsigned char));
        
        //packet.transac_id = 0;
        packet.func_code = 3;
        packet.length = 7;

        // Accept connections
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            printf("Connection failed");
        }
        else {
            printf("Connection accepted\n");
            while(1){
                // Allocate memory for buffers, since the data package length varies
                unsigned char *buff_recv = (unsigned char *)malloc(BUF_SIZE * sizeof(unsigned char));
                unsigned char *buff_sent = (unsigned char *)malloc(BUF_SIZE * sizeof(unsigned char));
                //buff_sent[0] = transac_id_ptr[MSB];
                //buff_sent[1] = transac_id_ptr[LSB];
                buff_sent[2] = (unsigned char)(MSBYTE(packet.prot_id));
                buff_sent[3] = (unsigned char)(LSBYTE(packet.prot_id));
                buff_sent[4] = (unsigned char)(MSBYTE(packet.length));
                buff_sent[5] = (unsigned char)(LSBYTE(packet.length));
                buff_sent[6] = packet.unit_id;
                buff_sent[7] = packet.func_code;
                buff_sent[8] = 0x04;
                _ssize_t bytes_recv;
                if((bytes_recv = read(new_socket, buff_recv, BUF_SIZE)) > 0) {
                    buff_sent[0] = buff_recv[0];
                    buff_sent[1] = buff_recv[1];
                    buff_sent[9] = (unsigned char)(LSBYTE(registers[buff_recv[9]]));
                    buff_sent[10] = (unsigned char)(MSBYTE(registers[buff_recv[9]]));
                    buff_sent[11] = (unsigned char)(LSBYTE(registers[buff_recv[9+1]]));
                    buff_sent[12] = (unsigned char)(MSBYTE(registers[buff_recv[9+1]]));

                    printf("Client message: 0x");
                    for(int i = 0; i < bytes_recv; i++){
                        printf("%02X ", (unsigned char)buff_recv[i]);
                    }
                    printf("\n");
                    printf("Server response: 0x");
                    for(int i = 0; i <= 12; i++){
                        printf("%02X ", (unsigned char)buff_sent[i]);
                    }
                    printf("\n");
                    send(new_socket, buff_sent, 13, 0);
                }
                free(buff_sent);
                free(buff_recv);
                //free(packet.data);
            }
        }
    }

    return 0;
}