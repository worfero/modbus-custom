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

// declare registers as global variables
short registers[2200] = {0};

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

// fills some of the response bytes according to client's request
struct ModbusFrame modbus_frame(unsigned char *buff_recv) {
    struct ModbusFrame packet;
    // allocate memory for data section of the packet
    packet.data = (unsigned char *)malloc((BUF_SIZE - 8) * sizeof(unsigned char));
    // transaction ID = first two bytes of client request
    packet.transac_id = TO_SHORT(buff_recv[0], buff_recv[1]);
    // protocol ID is always zero for modbus
    packet.prot_id = 0;
    // server unit ID
    packet.unit_id = 1;
    // function code provided by the client's request 7th byte
    packet.func_code = buff_recv[7];

    return packet;
}

unsigned char *read_holding_registers(struct ModbusFrame packet, int size) {
    unsigned char *buffer = (unsigned char *)malloc(size * sizeof(unsigned char));
    // creating response message
    buffer[0] = (unsigned char)(MSBYTE(packet.transac_id));
    buffer[1] = (unsigned char)(LSBYTE(packet.transac_id));
    buffer[2] = (unsigned char)(MSBYTE(packet.prot_id));
    buffer[3] = (unsigned char)(LSBYTE(packet.prot_id));
    buffer[4] = (unsigned char)(MSBYTE(packet.length));
    buffer[5] = (unsigned char)(LSBYTE(packet.length));
    buffer[6] = packet.unit_id;
    buffer[7] = packet.func_code;
    buffer[8] = packet.data_length;
    for(int i=0; i < packet.data_length; i++) {
        buffer[9+i] = packet.data[i];
    }
    
    return buffer;
}

int main() {
    int server_fd = server_setup();
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

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
            // Allocate memory for client message buffer, since the data package length varies
            unsigned char *buff_recv = (unsigned char *)malloc(BUF_SIZE * sizeof(unsigned char));
            // Declaring pointer to server response buffer, which memory will be allocated later
            unsigned char *buff_sent;
            _ssize_t bytes_recv;
            if((bytes_recv = read(new_socket, buff_recv, BUF_SIZE)) > 0) {
                packet = modbus_frame(buff_recv);
                // Read Holding Registers - FC3
                if (packet.func_code == 3) {
                    // data length is 2 times the number of registers (11th byte of client request)
                    packet.data_length = buff_recv[11]*2;
                    // packet length is data section length plus the 3 previous bytes
                    packet.length = packet.data_length + 3;
                    // data fetched from desired registers
                    for(int i=0; i < packet.data_length/2; i++) {
                        packet.data[i*2] = (unsigned char)(LSBYTE(registers[buff_recv[9]+i]));
                        packet.data[(i*2)+1] = (unsigned char)(MSBYTE(registers[buff_recv[9]+i]));
                    }
                    // total message length
                    int size = packet.length + 6;
                    buff_sent = read_holding_registers(packet, size);
                }

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
                _ssize_t res_size = packet.length + 6;
                send(new_socket, buff_sent, res_size, 0);
            }
            free(buff_sent);
            free(buff_recv);
            free(packet.data);
        }
    }

    return 0;
}