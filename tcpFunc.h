#ifndef TCPFUNC_H
#define TCPFUNC_H
#include <stdint.h>
#include <stdio.h>
#include <cstring>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#define BUFFERSIZE 5240
#define HEADERSIZE 12
#define MSS 524
struct header{
    uint16_t dest_port;
    uint16_t seqnum;
    uint16_t acknum;
    uint8_t flags;
    char padding[5];
};
bool getACK(uint8_t flags);
bool getFIN(uint8_t flags);
bool getSYN(uint8_t flags);
void setACK(uint8_t* flags);
void setFIN(uint8_t* flags);
void setSYN(uint8_t* flags);
void resetFLAG(uint8_t* flags);
void initConn(struct header*h);
int cnct_server(int, char*, struct header*, char*, struct sockaddr_in*, socklen_t*);
int cnct_client(int, char*, struct header*, char*, struct sockaddr_in*, socklen_t*, int);
int cls_init(int, char*, struct header*, char*, struct sockaddr_in*, socklen_t*);
int cls_resp1(int, char*, struct header*, char*, struct sockaddr_in*, uint16_t);
int cls_resp2(int, char*, struct header*, char*, struct sockaddr_in*, uint16_t);
int wait_cls(int);
int readPacket(struct header* h, char* payload, int l, char* packet);
int writePacket(struct header* h, char* payload, int l, char* packet);
#endif