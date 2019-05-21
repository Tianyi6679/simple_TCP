#ifndef PACKET_H
#define PACKET_H
#include <stdio.h>
#include <cstring>
#include <string.h>
#include "header.h"

int readPacket(struct header* h, char* payload, int l, char* packet){
    std::memcpy(h, packet, sizeof(struct header));
    std::memcpy(payload, (packet + sizeof(struct header)),l);
    return sizeof(struct header) + strlen((const char*)payload);
}
int writePacket(struct header* h, char* payload, int l, char* packet){
    std::memcpy(packet, h, sizeof(struct header));
    std::memcpy((packet + sizeof(struct header)), payload, l);
    return sizeof(struct header) + strlen((const char*)payload);
}
#endif 