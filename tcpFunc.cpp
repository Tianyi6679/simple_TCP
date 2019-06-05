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
             <<flags<<std::endl;
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
    bool connected = false;
    while (!connected){
        recvfrom(sockfd,(char *)buffer, MSS, MSG_WAITALL, (struct sockaddr*)c_addr, len);
        readPacket(h, payload, 0, buffer);
        logging(RECV, h, 0, 0);
        if (getSYN(h->flags)){
            //std::cout<<"SYN received "<<ntohs(((struct sockaddr_in*)c_addr)->sin_port)<<" "<<((struct sockaddr_in*)c_addr)->sin_addr.s_addr<<std::endl;
            h->acknum = h->seqnum + 1;
            h->seqnum = 0;
            setACK(&(h->flags));
            connected = true;
        }     
    }
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 0, (const struct sockaddr *)c_addr, *len);
    logging(SEND, h, 0, 0);
    //std::cout<<"Connection Established "<<((struct sockaddr_in*)c_addr)->sin_port<<" "<<((struct sockaddr_in*)c_addr)->sin_addr.s_addr<<std::endl;
    return 0;
}

int cnct_client(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* servaddr, socklen_t* len, int port){
    //step 1
    initConn(h,port);
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 
        MSG_CONFIRM, (const struct sockaddr *) servaddr,  
            sizeof(*servaddr)); 
    logging(SEND, h, h->cwnd, 0);
    //std::cout << "Send SYN" <<std::endl;
    // step 2
    bool connected = false;
    while(!connected){
        recv(sockfd,(char *)buffer, MSS, 0);
        readPacket(h, payload, 0, buffer);
        logging(RECV, h, h->cwnd, 0);
        if (getSYN(h->flags) && getACK(h->flags) && h->acknum == 1){
            //std::cout<<"ACK received"<<std::endl;
            connected = true;
        }
        else{
            initConn(h,port);
            writePacket(h, payload, 0, buffer);
            sendto(sockfd, (const char *)buffer, MSS, 
                    MSG_CONFIRM, (const struct sockaddr *) servaddr,  
                    sizeof(*servaddr)); 
            logging(SEND, h, h->cwnd, 0);
            //std::cout << "Resend SYN" <<std::endl;
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
int cls_init(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* addr, socklen_t* len){
    resetFLAG(&(h->flags));
    setFIN(&(h->flags));
    h->acknum = 0;
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
    logging(SEND, h, 0, 0);
    //w8 for ack
    int expectack= h->seqnum + 1;
    //std::cout << "Close Request Sent!" << std::endl;
    while(recv(sockfd, (char*)buffer, MSS, 0) > 0){
        readPacket(h, payload, 0, buffer);
        logging(RECV, h, 0, 0);
        if (getACK(h->flags) && h->acknum == expectack ){
            break;
        }
    }
    //std::cout << "Receive first ACK" << std::endl;
    /* wait for the other party sending FIN */
    struct pollfd ufd[1];
    ufd[0].fd = sockfd;
    ufd[0].events = POLLIN;
    int pret;
    int timeout = WaitCLS;
    Timer t; 
    t.start();
    while(poll(ufd, 1, timeout) > 0){
        recv(sockfd, (char*)buffer, MSS, 0);
        readPacket(h, payload, 0, buffer);
        logging(RECV, h, 0, 0);
        if (getFIN(h->flags)){
            resetFLAG(&(h->flags));
            setACK(&(h->flags));
            h->acknum = h->seqnum + 1;
            writePacket(h, payload, 0, buffer);
            sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
            logging(SEND, h, 0, 0);
            //std::cout << "Receive FIN and ACK Back and Wait 2 Secs" << std::endl;
        }
        if ( (timeout = WaitCLS - t.elapsed()) <= 0) break;  
    }
    t.stop();
    std::cout<<"Wait Close ends"<<std::endl;
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
    resetFLAG(&(h->flags));
    setFIN(&(h->flags));
    h->seqnum = seqnum;
    h->acknum = 0;
    writePacket(h, payload, 0, buffer);
    sendto(sockfd, (const char*)buffer, MSS, MSG_CONFIRM, (const struct sockaddr*) addr, sizeof(*addr));
    logging(SEND, h, 0, 0);
    //std::cout<<"Send Second Fin" << std::endl;
    while(recv(sockfd, (char*)buffer, MSS, 0) > 0){
        readPacket(h, payload, 0, buffer);
        logging(RECV, h, 0, 0);
        if (getACK(h->flags) && h->acknum == seqnum + 1){
            //std::cout<<"Receive ACK to Second FIN"<<std::endl;
            break;
        }
    }
    return 0;
}
/* Right now only put this thread into sleep for 2 secs */
int wait_cls(int timeout){
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}


int sendFile (char* filename, int sockfd, uint16_t port, struct sockaddr *c_addr, socklen_t addr_len, bool debug) {
    // Read from the file
    std::ifstream req_file(filename, std::ios::binary | std::ios::ate);

    // Get the file length
    int len = req_file.tellg();
    // Reset the pointer to the beginning of the file to read from it
    req_file.seekg(0, std::ios::beg);
    // Initialize buffer we'll read into and send over the socket
    char resp_buffer[MSS];
    memset(resp_buffer, 0, MSS);
    // And the one for the packet payload
    char p_buff[PAYLOAD];
    memset(p_buff, 0, PAYLOAD);
    // And the one we'll read from
    char recv_buff[BUFFERSIZE];
    memset(recv_buff, 0, BUFFERSIZE);

    // Initialize timer, seqnum, acknum, congestion control, ack counter
    Timer rto;
    uint16_t seqnum = 0;
    uint16_t acknum = 0;
    uint16_t cwnd = MSS;
    std::list<Packet> unacked_p;
    CongestionControl congestion_manager;

    // Monitor socket for input

    struct pollfd s_poll[1] ;

    s_poll[0].fd = sockfd;
    s_poll[0].events = POLLIN;

    // Until we're done reading
    while (1){
        if (debug){
            std::cout << "Reading!\n";
            }
        req_file.read(p_buff, PAYLOAD);
        // How many bytes did we actually read?
        std::streamsize count = req_file.gcount();
        if (debug){
            char bytes_read[128];
            sprintf(bytes_read, "Bytes Read: %ld\n", count);
            std::cout << bytes_read << std::endl;
        }
        if (count == 0){
            if (debug){
            std::cout << "Done reading!\n";
            }
            req_file.close();
            memset(resp_buffer, 0, MSS);
            break;
        }
        // Prepare Packet for Sending
        if (!unacked_p.empty()){
            Packet last_packet_sent = unacked_p.back();
            uint16_t last_seqnum = last_packet_sent.h_seqnum();
            uint16_t last_acknum = last_packet_sent.h_acknum();
            Packet p(port, last_seqnum, last_acknum, cwnd, resp_buffer, count, 0);

            // Send the packet
            Header out_header = p.p_header();
            char* out_payload = p.p_data();
            int out_len = p.payload_len();
            writePacket(&out_header, out_payload, out_len, resp_buffer);

            // Start timer
            rto.start();

            // Add packet to unacknowledged list
            unacked_p.push_back(p);
        }
        else{
            Packet p(port, 0, 0, cwnd, resp_buffer, count, 0);
            // Send the packet
            Header out_header = p.p_header();
            char* out_payload = p.p_data();
            int out_len = p.payload_len();
            writePacket(&out_header, out_payload, out_len, resp_buffer);

            // Start timer
            rto.start();

            // Add packet to unacknowledged list
            unacked_p.push_back(p);
        }

        int sock_event = 0;

        if (sock_event = poll(s_poll, 1, RTO) > 0){
            if (s_poll[0].revents & POLLIN){
                /*Receive Packet*/
                int recv_count;
                Header recv_h;
                char recv_p[MSS];
                if (recv_count = recvfrom(sockfd, (char *)recv_buff, BUFFERSIZE, MSG_WAITALL, 
                    (struct sockaddr*) &c_addr, &addr_len) != -1){
                        readPacket(&recv_h, recv_p, 0, recv_p);
                        /*Were we expecting this ack num?*/
                        std::list<Packet>::const_iterator packet_iter = unacked_p.begin();
                        uint16_t recv_ack = recv_h.acknum;
                        while(packet_iter!=unacked_p.end()){
                            uint16_t cur_ack = packet_iter->h_seqnum() + packet_iter->payload_len();
                            if (cur_ack == recv_ack){
                                // Clear packet from unreceived acks
                                unacked_p.erase(packet_iter);
                                // Restart timer
                                rto.start();
                                // Update ssthresh and cwnd
                                congestion_manager.update();
                            }
                            else{
                                packet_iter++ ;
                            }

                        }
                    }
            }
        }
        else{
            congestion_manager.timeout();
        }

        if (debug){
          std::cout << resp_buffer << std::endl;
        }
    }
}
    