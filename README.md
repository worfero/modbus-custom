# modbus-custom
Modbus server developed from scratch over a TCP connection so I can have a better understanding of the protocol and also practice my C skills.

For now, it is capable of processing Read Holding Register, Write Holding Registers and Read Input Registers (FC 03, 16 and 04 respectively) requests of any size.

I have declared an array of each type of registers and set the some default values to them just for testing.

I am using Modbus Doctor client for testing but it should work with any other client.