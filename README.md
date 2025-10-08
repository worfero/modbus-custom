# modbus-custom
Modbus server developed from scratch over a TCP connection so I can have a better understanding of the protocol and also practice my C skills.

For now, it is capable of processing Read Holding Register, Write Holding Registers, Read Input Registers and Read Coils requests of up to 120 registers in size.

I have declared an array of each type of registers and set some default values to them just for testing.

I am using Modbus Doctor client for testing but it should work with any other client.