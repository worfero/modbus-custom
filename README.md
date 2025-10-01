# modbus-custom
Modbus server developed from scratch over a TCP connection so I can have a better understanding of the protocol and also practice my C skills.

For now, it is only capable of processing Read Holding Register requests of any size. I have declared an array of registers and set the value of the first 100 ones to their corresponding index.

I am using Modbus Doctor client for testing but it should work with any other client.