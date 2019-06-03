#include "tcpFunc.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#define BUFFERSIZE 5240
#define HEADERSIZE 12
#define MSS 524
//if error return -1 
//otherwise return 0
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

bool getACK(uint8_t flags) {return flags >> 7;}
bool getFIN(uint8_t flags) {return (flags & (1 << 6)) != 0;}
bool getSYN(uint8_t flags) {return (flags & (1 << 5)) != 0;}
void setACK(uint8_t* flags){*flags = *flags | (1<<7);}
void setFIN(uint8_t* flags){*flags = *flags | (1<<6);}
void setSYN(uint8_t* flags){*flags = *flags | (1<<5);}
void resetFLAG(uint8_t* flags) {*flags = 0;}
void initConn(struct header* h, int port){
    h->seqnum = 0;
    h->acknum = 0;
    h->flags = 0;
    h->dest_port = port;
    setSYN(&(h->flags));
}

int cnct_server(int sockfd, char* buffer, struct header* h, char* payload, struct sockaddr_in* c_addr, socklen_t* len){
    bool connected = false;
    while (!connected){
        recvfrom(sockfd,(char *)buffer, MSS, MSG_WAITALL, (struct sockaddr*)c_addr, len);
        readPacket(h, payload, 0, buffer);
        if (getSYN(h->flags)){
            std::cout<<"SYN received "<<ntohs(((struct sockaddr_in*)c_addr)->sin_port)<<" "<<((struct sockaddr_in*)c_addr)->sin_addr.s_addr<<std::endl;
            h->acknum = h->seqnum + 1;
            h->seqnum = 0;
            setACK(&(h->flags));
            connected = true;
        }     
    }
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 0, (const struct sockaddr *)c_addr, *len);
    std::cout<<"Connection Established "<<((struct sockaddr_in*)c_addr)->sin_port<<" "<<((struct sockaddr_in*)c_addr)->sin_addr.s_addr<<std::endl;
    return 0;
}

int cnct_client(int sockfd, char* buffer, struct header* h, char* payload, struct sockaddr_in* servaddr, socklen_t* len, int port){
    //step 1
    initConn(h,port);
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 
        MSG_CONFIRM, (const struct sockaddr *) servaddr,  
            sizeof(*servaddr)); 
    std::cout << "Send SYN" <<std::endl;
    // step 2
    bool connected = false;
    while(!connected){
        recv(sockfd,(char *)buffer, MSS, 0);
        readPacket(h, payload, 0, buffer);
        if (getSYN(h->flags) && getACK(h->flags) && h->acknum == 1){
            std::cout<<"ACK received"<<std::endl;
            connected = true;
        }
        else{
            initConn(h,port);
            writePacket(h, payload, 0, buffer);
            sendto(sockfd, (const char *)buffer, MSS, 
                    MSG_CONFIRM, (const struct sockaddr *) servaddr,  
                    sizeof(*servaddr)); 
            std::cout << "Resend SYN" <<std::endl;
        }
    }
    // step 3
    std::cout<<"Connection Established"<<std::endl;
    if (connect(sockfd, (struct sockaddr *) servaddr, sizeof(*servaddr)) == -1){
        perror("ERROR: connect failed");
        exit(1);
    }
    return 0;
}
/* actions of initiating a close request */
int cls_init(int sockfd, char* buffer, struct header* h, char* payload, struct sockaddr_in* addr, socklen_t* len){
    resetFLAG(&(h->flags));
    setFIN(&(h->flags));
    h->acknum = 0;
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
    //w8 for ack
    int expectack= h->seqnum + 1;
    while(recv(sockfd, (char*)buffer, MSS, 0) > 0){
        readPacket(h, payload, 0, buffer);
        if (getACK(h->flags) && h->acknum == expectack ){
            break;
        }
    }
    /* wait for the other party sending FIN*/
    while(recv(sockfd, (char*)buffer, MSS, 0) > 0){
        readPacket(h, payload, 0, buffer);
        if (getFIN(h->flags)){
            resetFLAG(&(h->flags));
            setACK(&(h->flags));
            h->acknum = h->seqnum + 1;
            writePacket(h, payload, 0, buffer);
            sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
            wait_cls(2);
        }
    }
    return 0;     
}
/* actions of respond to a close request */
int cls_resp1(int sockfd, char* buffer, struct header* h, char* payload, struct sockaddr_in* addr, uint16_t seqnum){
    resetFLAG(&(h->flags));
    setACK(&(h->flags));
    h->acknum = h->seqnum + 1;
    h->seqnum = seqnum;
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
    return 0;
}
/* actions of sending last ACK and wait for closing*/
int cls_resp2(int sockfd, char* buffer, struct header* h, char* payload, struct sockaddr_in* addr, uint16_t seqnum){
    resetFLAG(&(h->flags));
    setFIN(&(h->flags));
    h->seqnum = seqnum;
    h->acknum = 0;
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
    while(recv(sockfd, (char*)buffer, MSS, 0) > 0){
        readPacket(h, payload, 0, buffer);
        if (getACK(h->flags) && h->acknum == seqnum + 1){
            break;
        }
    }
    return 0;
}
/* Right now only put this thread into sleep for 2 secs*/
int wait_cls(int timeout){
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}