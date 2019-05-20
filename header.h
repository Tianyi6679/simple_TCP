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

#endif