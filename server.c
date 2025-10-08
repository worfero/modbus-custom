#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 502
#define BUF_SIZE 128

// function codes
#define READ_COILS 0x01
#define READ_DISCRETE_INPUTS 0x02
#define READ_HOLDING_REGISTERS 0x03
#define READ_INPUT_REGISTERS 0x04
#define WRITE_SINGLE_COIL 0x05
#define WRITE_SINGLE_HOLDING_REGISTER 0x06
#define WRITE_COILS 0x0F
#define WRITE_HOLDING_REGISTERS 0x10

// message bytes
#define TRAN_ID_MSB 0
#define TRAN_ID_LSB 1
#define PROT_ID_MSB 2
#define PROT_ID_LSB 3
#define LENGTH_MSB 4
#define LENGTH_LSB 5
#define UNIT_ID 6
#define F_CODE 7
// exception byte
#define EXCEPTION 8
// data length for reading registers byte
#define DATA_LENGTH 8
// address of registers to be written bytes
#define ADDRESS_MSB 8
#define ADDRESS_LSB 9
// quantity of registers to be written in sequence bytes
#define QUANTITY_MSB 10
#define QUANTITY_LSB 11
// data to be returned to client on reading operations, variable size
#define DATA(x) (x)

// exception codes
#define ILLEGAL_FC 0x01

// most significante and less significant byte macros
#define LSBYTE(x) ((x << 8) & 0xFF)
#define MSBYTE(x) ((x) & 0xFF)

// round division up macro
#define CEIL(x, y) ((x + y - 1) / y)

// two char to short conversion macro
#define TO_SHORT(x, y) (((short)x) << 8) | y

struct ModbusFrame {
    // MBAP Header
    short transac_id;
    short prot_id;
    short length;
    unsigned char unit_id;
    // Application layer
    unsigned char func_code;
    short written_address;
    short written_quantity;
    unsigned char data_length;
    unsigned char exception;
    unsigned char *data;
};

// declare registers as global variables
short holding_registers[2000] = {0};
short input_registers[2000] = {0};
bool coils[2000] = {0};
bool discrete_inputs[2000] = {0};


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

void read_holding_registers(struct ModbusFrame *packet, unsigned char *buff_recv) {
    // data length is 2 times the number of registers (11th byte of client request)
    packet->data_length = buff_recv[11]*2;
    // allocate memory for data section of the packet
    packet->data = (unsigned char *)calloc((packet->data_length), sizeof(unsigned char));
    // packet length is data section length plus the 3 previous bytes
    packet->length = packet->data_length + 3;
    // data fetched from desired registers
    for(int i=0; i < packet->data_length/2; i++) {
        packet->data[i*2] = (unsigned char)(LSBYTE(holding_registers[buff_recv[9]+i]));
        packet->data[(i*2)+1] = (unsigned char)(MSBYTE(holding_registers[buff_recv[9]+i]));
    }
}

void read_input_registers(struct ModbusFrame *packet, unsigned char *buff_recv) {
    // data length is 2 times the number of registers (11th byte of client request)
    packet->data_length = buff_recv[11]*2;
    // allocate memory for data section of the packet
    packet->data = (unsigned char *)calloc((packet->data_length), sizeof(unsigned char));
    // packet length is data section length plus the 3 previous bytes
    packet->length = packet->data_length + 3;
    // data fetched from desired registers
    for(int i=0; i < packet->data_length/2; i++) {
        packet->data[i*2] = (unsigned char)(LSBYTE(input_registers[buff_recv[9]+i]));
        packet->data[(i*2)+1] = (unsigned char)(MSBYTE(input_registers[buff_recv[9]+i]));
    }
}

void write_holding_registers(struct ModbusFrame *packet, unsigned char *buff_recv) {
    // data length is always 4 bytes for write multiple holding registers. 2 for the starting address and 2 for the quantity
    packet->data_length = 4;
    // packet length is data section length plus the 3 previous bytes
    packet->length = packet->data_length + 3;
    // get address to be written from bytes 8 and 9 of client request
    packet->written_address = TO_SHORT(buff_recv[8], buff_recv[9]);
    // get quantity of written addresses in sequence from bytes 10 and 11 of client request
    packet->written_quantity = TO_SHORT(buff_recv[10], buff_recv[11]);
    // data to be written to desired registers
    for(int i=0; i < packet->written_quantity; i++) {
        holding_registers[(packet->written_address + i)] = TO_SHORT(buff_recv[13+(2*i)], buff_recv[14+(2*i)]);
    }
}

void read_coils(struct ModbusFrame *packet, unsigned char *buff_recv) {
    // number of coils to be read is 11th byte of client request
    unsigned char number_of_coils = buff_recv[11];
    // data length is the division of the number of coils to be read by 8 (char size) rounded up
    packet->data_length = CEIL(number_of_coils, 8);
    // allocate memory for data section of the packet
    packet->data = (unsigned char *)calloc((packet->data_length), sizeof(unsigned char));
    // packet length is data section length plus the 3 previous bytes
    packet->length = packet->data_length + 3;
    // data fetched from desired coils
    for(int i=0; i < packet->data_length; i++) {
        for (int j = 0; j < 8; ++j) {
            if (coils[j+(i*8)] && (number_of_coils > 0)) {
                // If the coil is true, set the bit on the data section array
                packet->data[i] |= (1 << j); 
            }
            if(number_of_coils > 0) {
                number_of_coils--;
            }
        }
    }
}

void exception(struct ModbusFrame *packet, unsigned char *buff_recv) {
    // generate exception function code (FC + 128 according to modbus definition)
    packet->func_code = packet->func_code + 0x80;
    // exception code 01 - function code not found
    packet->exception = ILLEGAL_FC;
    // for exceptions, length is always 6
    packet->length = 6;

    printf("Illegal function code, request was not successful\n");
}

struct ModbusFrame modbus_frame(unsigned char *buff_recv) {
    struct ModbusFrame packet;
    // transaction ID = first two bytes of client request
    packet.transac_id = TO_SHORT(buff_recv[0], buff_recv[1]);
    // protocol ID is always zero for modbus
    packet.prot_id = 0;
    // server unit ID
    packet.unit_id = 1;
    // function code provided by the client's request 7th byte
    packet.func_code = buff_recv[7];

    // Read Holding Registers
    if (packet.func_code == READ_HOLDING_REGISTERS) {
        read_holding_registers(&packet, buff_recv);
    }
    // Read Input Registers
    else if (packet.func_code == READ_INPUT_REGISTERS) {
        read_input_registers(&packet, buff_recv);
    }
    // Write Holding Registers
    else if (packet.func_code == WRITE_HOLDING_REGISTERS) {
        write_holding_registers(&packet, buff_recv);
    }
    else if (packet.func_code == READ_COILS) {
        read_coils(&packet, buff_recv);
    }
    // Illegal function code
    else {
        exception(&packet, buff_recv);
    }

    return packet;
}

unsigned char *read_response(struct ModbusFrame packet, int size) {
    unsigned char *buffer = (unsigned char *)malloc(size * sizeof(unsigned char));
    // creating response message
    buffer[TRAN_ID_MSB] = (unsigned char)(MSBYTE(packet.transac_id));
    buffer[TRAN_ID_LSB] = (unsigned char)(LSBYTE(packet.transac_id));
    buffer[PROT_ID_MSB] = (unsigned char)(MSBYTE(packet.prot_id));
    buffer[PROT_ID_LSB] = (unsigned char)(LSBYTE(packet.prot_id));
    buffer[LENGTH_MSB] = (unsigned char)(MSBYTE(packet.length));
    buffer[LENGTH_LSB] = (unsigned char)(LSBYTE(packet.length));
    buffer[UNIT_ID] = packet.unit_id;
    buffer[F_CODE] = packet.func_code;
    buffer[DATA_LENGTH] = packet.data_length;
    for(int i=0; i < packet.data_length; i++) {
        buffer[DATA(i+9)] = packet.data[i];
    }
    free(packet.data);
    
    return buffer;
}

unsigned char *write_response(struct ModbusFrame packet, int size) {
    unsigned char *buffer = (unsigned char *)malloc(size * sizeof(unsigned char));
    // creating response message
    buffer[TRAN_ID_MSB] = (unsigned char)(MSBYTE(packet.transac_id));
    buffer[TRAN_ID_LSB] = (unsigned char)(LSBYTE(packet.transac_id));
    buffer[PROT_ID_MSB] = (unsigned char)(MSBYTE(packet.prot_id));
    buffer[PROT_ID_LSB] = (unsigned char)(LSBYTE(packet.prot_id));
    buffer[LENGTH_MSB] = (unsigned char)(MSBYTE(packet.length));
    buffer[LENGTH_LSB] = (unsigned char)(LSBYTE(packet.length));
    buffer[UNIT_ID] = packet.unit_id;
    buffer[F_CODE] = packet.func_code;
    buffer[ADDRESS_MSB] = (unsigned char)(MSBYTE(packet.written_address));
    buffer[ADDRESS_LSB] = (unsigned char)(LSBYTE(packet.written_address));
    buffer[QUANTITY_MSB] = (unsigned char)(MSBYTE(packet.written_quantity));
    buffer[QUANTITY_LSB] = (unsigned char)(LSBYTE(packet.written_quantity));

    return buffer;
}

unsigned char *exception_response(struct ModbusFrame packet, int size) {
    unsigned char *buffer = (unsigned char *)malloc(size * sizeof(unsigned char));
    // creating response message
    buffer[TRAN_ID_MSB] = (unsigned char)(MSBYTE(packet.transac_id));
    buffer[TRAN_ID_LSB] = (unsigned char)(LSBYTE(packet.transac_id));
    buffer[PROT_ID_MSB] = (unsigned char)(MSBYTE(packet.prot_id));
    buffer[PROT_ID_LSB] = (unsigned char)(LSBYTE(packet.prot_id));
    buffer[LENGTH_MSB] = (unsigned char)(MSBYTE(packet.length));
    buffer[LENGTH_LSB] = (unsigned char)(LSBYTE(packet.length));
    buffer[UNIT_ID] = packet.unit_id;
    buffer[F_CODE] = packet.func_code;
    buffer[EXCEPTION] = packet.exception;

    return buffer;
}

int main() {
    int server_fd = server_setup();
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    for(int i = 0; i < 101; i++) {
        holding_registers[i] = i;
        input_registers[i] = 2*i;
        coils[i] = i%2;
        discrete_inputs[i] = i%2;
    }

    struct ModbusFrame packet;

    while(1){
        // Accept connections
        if((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            printf("Connection failed\n");
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
                    // fills some of the response bytes according to client's request
                    packet = modbus_frame(buff_recv);

                    // total response message length
                    int size = packet.length + 6;

                    // Read operation buffer
                    if (packet.func_code == READ_HOLDING_REGISTERS || packet.func_code == READ_INPUT_REGISTERS || packet.func_code == READ_COILS) {
                        buff_sent = read_response(packet, size);
                    }

                    // Write operation buffer
                    else if (packet.func_code == WRITE_HOLDING_REGISTERS) {
                        buff_sent = write_response(packet, size);
                    }

                    // Illegal function code exception
                    else {
                        buff_sent = exception_response(packet, size);
                    }

                    _ssize_t res_size = packet.length + 6;

                    printf("Client message: 0x");
                    for(int i = 0; i < bytes_recv; i++){
                        printf("%02X ", (unsigned char)buff_recv[i]);
                    }
                    printf("\n");
                    printf("Server response: 0x");
                    for(int i = 0; i <= res_size; i++){
                        printf("%02X ", (unsigned char)buff_sent[i]);
                    }
                    printf("\n");
                    send(new_socket, buff_sent, res_size, 0);
                    free(buff_sent);
                }
                else{
                    free(buff_recv);
                    printf("Connection lost...\n");
                    break;
                }
                free(buff_recv);
            }
        }
    }

    return 0;
}