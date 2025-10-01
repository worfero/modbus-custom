#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 502
#define BUF_SIZE 128

#define LSBYTE(x) ((x << 8) & 0xFF)
#define MSBYTE(x) ((x) & 0xFF)

#define TO_SHORT(x, y) (((short)x) << 8) | y

struct ModbusFrame {
    // MBAP Header
    short transac_id;
    short prot_id;
    short length;
    unsigned char unit_id;
    // Application layer
    unsigned char func_code;
    unsigned char data_length;
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

    char unitID = 1;
    short registers[2200] = {0};
    for(int i = 0; i < 101; i++) {
        registers[i] = i;
    }

    struct ModbusFrame packet;

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
            _ssize_t bytes_recv;
            if((bytes_recv = read(new_socket, buff_recv, BUF_SIZE)) > 0) {
                // allocate memory for data section of the packet
                packet.data = (unsigned char *)malloc((BUF_SIZE - 8) * sizeof(unsigned char));

                // transaction ID = first two bytes of client request
                packet.transac_id = TO_SHORT(buff_recv[0], buff_recv[1]);
                // protocol ID is always zero for modbus
                packet.prot_id = 0;
                // packet length manually tuned for now
                packet.length = 7;
                // server unit ID
                packet.unit_id = unitID;
                // function code provided by the client's reponse 7th byte
                packet.func_code = buff_recv[7];
                // data length manually tuned for now
                packet.data_length = 0x04;
                //data manually added for now
                for(int i=0; i < 2; i++) {
                    packet.data[i*2] = (unsigned char)(LSBYTE(registers[buff_recv[9]+i]));
                    packet.data[(i*2)+1] = (unsigned char)(MSBYTE(registers[buff_recv[9]+i]));
                }

                buff_sent[0] = (unsigned char)(MSBYTE(packet.transac_id));
                buff_sent[1] = (unsigned char)(LSBYTE(packet.transac_id));
                buff_sent[2] = (unsigned char)(MSBYTE(packet.prot_id));
                buff_sent[3] = (unsigned char)(LSBYTE(packet.prot_id));
                buff_sent[4] = (unsigned char)(MSBYTE(packet.length));
                buff_sent[5] = (unsigned char)(LSBYTE(packet.length));
                buff_sent[6] = packet.unit_id;
                buff_sent[7] = packet.func_code;
                buff_sent[8] = packet.data_length;
                buff_sent[9] = (unsigned char)(LSBYTE(packet.data[0]));
                buff_sent[10] = (unsigned char)(MSBYTE(packet.data[1]));
                buff_sent[11] = (unsigned char)(LSBYTE(packet.data[2]));
                buff_sent[12] = (unsigned char)(MSBYTE(packet.data[3]));

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
            free(packet.data);
            //free(packet.data);
        }
    }

    return 0;
}