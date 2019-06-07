#include "tcpFunc.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <string>
//if error return -1 
//otherwise return 0
int readPacket(struct Header* h, char* payload, int l, char* packet){
    std::memcpy(h, packet, sizeof(struct Header));
    std::memcpy(payload, (packet + sizeof(struct Header)),l);
    return sizeof(struct Header) + strlen((const char*)payload);
}

int writePacket(struct Header* h, char* payload, int l, char* packet){
    std::memcpy(packet, h, sizeof(struct Header));
    std::memcpy((packet + sizeof(struct Header)), payload, l);
    return sizeof(struct Header) + strlen((const char*)payload);
}
/* TODO: add dup flag */
int logging(int flag, struct Header* h, int cwnd, int ssthresh){
    std::string action, flags;
    if (flag == RECV) action = "RECV";
    else action = "SEND";
    switch (h->flags){
        case 0:
            flags = "";
            break;
        case 128:
            flags = "ACK";
            break;
        case 64:
            flags = "FIN";
            break;
        case 32:
            flags = "SYN";
            break;
        case 192:
            flags = "ACK FIN";
            break;
        case 160:
            flags = "ACK SYN";
            break;
        case 96:
            flags = "SYN FIN";
            break;
        default:
            flags = "ACK SYN FIN";
    }
    std::cout<<action<<' '
             <<h->seqnum<<' '
             <<h->acknum<<' '
             <<cwnd<<' '
             <<ssthresh<<' '
             <<flags;
    if (h->dup == (uint16_t) true) std::cout<<" DUP";
    std::cout<<std::endl;
    return 0;
}
bool getACK(uint8_t flags) {return flags >> 7;}
bool getFIN(uint8_t flags) {return (flags & (1 << 6)) != 0;}
bool getSYN(uint8_t flags) {return (flags & (1 << 5)) != 0;}
void setACK(uint8_t* flags){*flags = *flags | (1<<7);}
void setFIN(uint8_t* flags){*flags = *flags | (1<<6);}
void setSYN(uint8_t* flags){*flags = *flags | (1<<5);}
void resetFLAG(uint8_t* flags) {*flags = 0;}
void initConn(struct Header* h, int port){
    h->seqnum = 0;
    h->acknum = 0;
    h->flags = 0;
    h->dest_port = port;
    setSYN(&(h->flags));
}

int cnct_server(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* c_addr, socklen_t* len){
    bool handshake = false;
    while (!handshake){
        recvfrom(sockfd,(char *)buffer, MSS, MSG_WAITALL, (struct sockaddr*)c_addr, len);
        readPacket(h, payload, 0, buffer);
        logging(RECV, h, 0, 0);
        if (getSYN(h->flags)){
            //std::cout<<"SYN received "<<ntohs(((struct sockaddr_in*)c_addr)->sin_port)<<" "<<((struct sockaddr_in*)c_addr)->sin_addr.s_addr<<std::endl;
            h->acknum = h->seqnum + 1;
            h->seqnum = 0;
            setACK(&(h->flags));
            handshake = true;
        }     
    }
    struct pollfd ufd[1];
    ufd[0].fd = sockfd;
    ufd[0].events = POLLIN;
    int pret;
    int timeout = RET_TO;
    Timer t; 
    bool connected = false;
    struct Header h2;
    //wait_cls(2);
    do{
        writePacket(h, payload, 0, buffer);
        sendto(sockfd, (const char *)buffer, MSS, 0, (const struct sockaddr *)c_addr, *len);
        logging(SEND, h, 0, 0);
        t.start();
        timeout = RET_TO;
        while(poll(ufd, 1, timeout) > 0){
            recvfrom(sockfd, buffer, MSS, MSG_WAITALL, (struct sockaddr*)c_addr, len);
            readPacket(&h2, payload, PAYLOAD, buffer);
            logging(RECV, &h2, 0, 0);
            if (getACK(h2.flags) && h2.acknum == h->seqnum + 1){
                connected = true;
                break;
            }
            if ( (timeout = RET_TO - t.elapsed()) <= 0) break;
        }
    } while(!connected);
    std::memcpy(h, &h2, sizeof(struct Header));
    //std::cout<<"Connection Established "<<((struct sockaddr_in*)c_addr)->sin_port<<" "<<((struct sockaddr_in*)c_addr)->sin_addr.s_addr<<std::endl;
    return 0;
}

int cnct_client(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* servaddr, socklen_t* len, int port,
 CongestionControl conman){
    //step 1
    struct pollfd ufd[1];
    ufd[0].fd = sockfd;
    ufd[0].events = POLLIN;
    int pret;
    int timeout = RET_TO;
    Timer t; 
    bool connected = false;
    do{
        initConn(h,port);
        writePacket(h, payload, 0, buffer);
        sendto(sockfd, (const char *)buffer, MSS, 0, (const struct sockaddr *)servaddr, sizeof(*servaddr));
        logging(SEND, h, 0, 0);
        t.start();
        timeout = RET_TO;
        while(poll(ufd, 1, timeout) > 0){
            recv(sockfd,(char *)buffer, MSS, 0);
            readPacket(h, payload, 0, buffer);
            logging(RECV, h, 0, 0);
            if (getSYN(h->flags) && getACK(h->flags) && h->acknum == 1){
            //std::cout<<"ACK received"<<std::endl;
                connected = true;
                //std::cout<<"Connection Established"<<std::endl;
                if (connect(sockfd, (struct sockaddr *) servaddr, sizeof(*servaddr)) == -1){
                    perror("ERROR: connect failed");
                    exit(1);
                }
                break;
            }
            if ( (timeout = RET_TO - t.elapsed()) <= 0) break;
        }
    } while(!connected);
    //std::cout << "Send SYN" <<std::endl;
    // step 2
    return 0;
}
/* actions of initiating a close request */
int cls_init(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* addr, socklen_t* len, uint16_t seqnum){
    
    struct pollfd ufd[1];
    ufd[0].fd = sockfd;
    ufd[0].events = POLLIN;
    int pret;
    int timeout = RET_TO;
    Timer t; 
    bool Fin_Ack = false;
    struct Header h2;
    resetFLAG(&(h->flags));
    setFIN(&(h->flags));
    h->acknum = 0;
    h->seqnum = seqnum;
    int expectack= h->seqnum + 1;
    int sendTimes = 0;
    do {
        writePacket(h, payload, 0, buffer);
        sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
        logging(SEND, h, 0, 0);
        //std::cout << "Close Request Sent!" << std::endl;
        t.start();
        while(poll(ufd, 1, timeout) > 0){
            recv(sockfd, (char*)buffer, MSS, 0);
            readPacket(&h2, payload, 0, buffer);
            logging(RECV, &h2, 0, 0);
            if (getACK(h2.flags) && h2.acknum == expectack ){
                Fin_Ack = true;
                break;
            }
            if ( (timeout = RET_TO - t.elapsed()) <= 0) break;
        }
        if (++sendTimes >= 20) return -1;
    } while(!Fin_Ack);
    //std::cout << "Receive first ACK" << std::endl;
    //wait_cls(2);
    /* wait for the other party sending FIN */
    t.start();
    timeout = WaitCLS;
    /* WAIT_CLS for 2 sec */
    while(poll(ufd, 1, timeout) > 0){
        recv(sockfd, (char*)buffer, MSS, 0);
        readPacket(h, payload, 0, buffer);
        if (getFIN(h->flags)){
            logging(RECV, h, 0, 0);
            resetFLAG(&(h->flags));
            setACK(&(h->flags));
            h->acknum = h->seqnum + 1;
            h->seqnum = expectack;
            writePacket(h, payload, 0, buffer);
            sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
            logging(SEND, h, 0, 0);
            //std::cout << "Receive FIN and ACK Back and Wait 2 Secs" << std::endl;
        }
        if ( (timeout = WaitCLS - t.elapsed()) <= 0) break;  
    }
    t.stop();
    //std::cout<<"Wait Close ends"<<std::endl;
    return 0;     
}
/* actions of respond to a close request */
int cls_resp1(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* addr, uint16_t seqnum){
    //std::cout << "Receive Fin" << std::endl;
    resetFLAG(&(h->flags));
    setACK(&(h->flags));
    h->acknum = h->seqnum + 1;
    h->seqnum = seqnum;
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
    logging(SEND, h, 0, 0);
    //std::cout << "Send ACK to FIN" << std::endl;
    return 0;
}
/* actions of sending last ACK and wait for closing */
int cls_resp2(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* addr, uint16_t seqnum){
    struct pollfd ufd[1];
    ufd[0].fd = sockfd;
    ufd[0].events = POLLIN;
    int pret;
    int timeout = RET_TO;
    Timer t; 
    resetFLAG(&(h->flags));
    setFIN(&(h->flags));
    h->seqnum = seqnum;
    h->acknum = 0;
    bool Fin_Ack = false;
    struct Header h2;
    int trial  = 0;
    do{
        writePacket(h, payload, 0, buffer);
        sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
        logging(SEND, h, 0, 0);
        //std::cout<<"Send Second Fin" << std::endl;
        t.start();
        while( poll(ufd, 1, timeout) > 0){
            recv(sockfd, (char*)buffer, MSS, 0);
            readPacket(&h2, payload, 0, buffer);
            logging(RECV, &h2, 0, 0);
            if (getACK(h2.flags) && h2.acknum == h->seqnum + 1){
                //std::cout<<"Receive ACK to Second FIN"<<std::endl;
                Fin_Ack = true;
                break;
            }
            else if (getFIN(h2.flags)){
                cls_resp1(sockfd, buffer, &h2, payload, addr, seqnum);
            }
            if ((timeout = RET_TO - t.elapsed()) <= 0) break;
        }
        if (++trial >= 5) return -1;
    }while(!Fin_Ack);
    
    return 0;
}
/* Right now only put this thread into sleep for 2 secs */
int wait_cls(int timeout){
    std::this_thread::sleep_for(std::chrono::seconds(timeout));
    return 0;
}

bool seqnum_comp(Packet a, Packet b){
    if ((b.h_seqnum() <= a.h_seqnum() && (a.h_seqnum() - b.h_seqnum()) <= MAXSEQNUM/2) || 
                    (b.h_seqnum() > a.h_seqnum() && (b.h_seqnum()- a.h_seqnum()) > MAXSEQNUM/2)){
        return false;
    }
    else return true;
}

    