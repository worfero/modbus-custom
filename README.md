# modbus-custom
Modbus server developed from scratch over a TCP connection so I can have a better understanding of the protocol and also practice my C skills.

It is capable of processing the following requests: 
    - Read Holding Register
    - Write Multiple Holding Registers
    - Read Input Registers
    - Read Coils
    - Write Multiple Coils
    - Read Discrete Inputs

Any other function code will return "invalid function code" exception. Other exceptions are not yet covered.

There is a limit of 120 registers per request, be it 16-bit or 1-bit.

I have declared an array of each type of registers and set some default values to them just for testing.

I am using Modbus Doctor client for testing but it should work with any other client.