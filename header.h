#ifndef HEADER_H
#define HEADER_H
#include <stdint.h>
struct header{
    uint16_t dest_port;
    uint16_t seqnum;
    uint16_t acknum;
    uint8_t flags;
    char padding[5];
};
/*
    FLAGS: ACK|FIN|SYN|PAD|PAD|PAD|PAD|PAD
*/
bool getACK(uint8_t flags) {return flags >> 7;}
bool getFIN(uint8_t flags) {return (flags & (1 << 6)) != 0;}
bool getSYN(uint8_t flags) {return (flags & (1 << 5)) != 0;}
void setACK(uint8_t* flags){*flags = *flags | (1<<7);}
void setFIN(uint8_t* flags){*flags = *flags | (1<<6);}
void setSYN(uint8_t* flags){*flags = *flags | (1<<5);}
#endif