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
                std::cout<<"Connection Established"<<std::endl;
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
int cls_init(int sockfd, char* buffer, struct Header* h, char* payload, struct sockaddr_in* addr, socklen_t* len){
    
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
    }while(!Fin_Ack);
    
    return 0;
}
/* Right now only put this thread into sleep for 2 secs */
int wait_cls(int timeout){
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}

bool seqnum_comp(Packet a, Packet b){
    if (a.h_seqnum() % MAXSEQNUM > b.h_seqnum() % MAXSEQNUM){
        return false;
    }
    else return true;
}
/*
int sendFile (char* filename, int sockfd, uint16_t port, struct sockaddr *c_addr, socklen_t addr_len, uint16_t init_seq,
uint16_t init_ack, uint16_t init_cwnd, bool debug) {
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
    uint16_t seqnum = init_seq;
    uint16_t acknum = init_ack;
    uint16_t cwnd = init_cwnd;
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
        int bytes_read = 0;
        while(bytes_read < congestion_manager.get_cwnd()){
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
            // Prepare Packet for Sending - add to list
            if (unacked_p.empty()){
                Packet p(port, 0, 0, 0, resp_buffer, count, 0);
                unacked_p.push_back(p);
            }
            else{
                Packet last_packet_added = unacked_p.back();
                uint16_t last_seqnum = last_packet_added.h_seqnum();
                uint16_t last_acknum = last_packet_added.h_acknum();
                Packet p(port, last_seqnum+1, last_acknum+1, cwnd, resp_buffer, count, 0);
                unacked_p.push_back(p);
            }
            // Update seqnum
            bytes_read += count;
        }

        // Send all the unsent packets

        std::list<Packet>::const_iterator packet_iter = unacked_p.begin();
        while(packet_iter!=unacked_p.end()){
            if (packet_iter->h_seqnum() > seqnum){
                Header out_header = p.p_header();
                char* out_payload = p.p_data();
                int out_len = p.payload_len();
                writePacket(&out_header, out_payload, out_len, resp_buffer);
                seqnum += MSS;
            }
        }

        int sock_event = 0;

        if (sock_event = poll(s_poll, 1, RTO) > 0){
            if (s_poll[0].revents & POLLIN){
                //Receive Packet//
                int recv_count;
                Header recv_h;
                char recv_p[MSS];
                if (recv_count = recvfrom(sockfd, (char *)recv_buff, BUFFERSIZE, MSG_WAITALL, 
                    (struct sockaddr*) &c_addr, &addr_len) != -1){
                        readPacket(&recv_h, recv_p, 0, recv_p);
                        //Were we expecting this ack num?//
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


int recvFile (char* filename, int sockfd, uint16_t port, struct sockaddr *c_addr, socklen_t addr_len, bool debug,
uint16_t init_seqnum, uint16_t init_acknum) {

    // Create the file to store data into
    int file_p = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);

    // Create the buffer we'll read into and send over socket
    char incoming[MSS];
    memset(incoming, 0, MSS);
    char outgoing[MSS];
    memset(outgoing, 0, MSS);

    //Initialize some constants
    int recv_bytes = 0;
    Packet* recv_p = NULL;
    std::list<Packet> buffered_p;
    uint16_t acknum = init_acknum;
    uint16_t seqnum = init_seqnum;

    // While data is received
    while(int count = recvfrom(sockfd, (char*) incoming, MSS, MSG_WAITALL, c_addr, &addr_len) != 0){
        if (count = -1){
            perror("Error reading from the socket");
            exit(-1);
        }
        // Else store the packet in an object
        recv_p = reinterpret_cast<Packet*>(incoming);
        Packet* new_p = new Packet(port, recv_p->h_seqnum(), recv_p->h_acknum(), recv_p->h_dup(), recv_p->p_payload(), 
        recv_p->payload_len(), recv_p->h_flags());

        // Is it the packet we expected?
        if (new_p->h_acknum() == acknum){
            int bytes_written = write(file_p, new_p->p_payload(), new_p->payload_len());
            if (bytes_written < 0){
                perror("Error: unable to write data to file\n");
            }
            acknum += bytes_written % MAXSEQNUM ;
            // Send ack for packet
            Header ack_header;
            ack_header.acknum = acknum;
            ack_header.seqnum = seqnum;
            int ack_len = HEADERSIZE;
            char* payload = "\0" ;
            writePacket(&ack_header, payload, ack_len, outgoing);
            sendto(sockfd, (const char *)outgoing, MSS, 0, (const struct sockaddr *)c_addr, sizeof(*c_addr));

            // Do any of the buffered packets now fall in order?
            std::list<Packet>::const_iterator npb = buffered_p.begin();
            while(npb != buffered_p.end()){
                while (!buffered_p.empty() && npb->h_acknum() == acknum){
                    int bytes_written = write(file_p, new_p->p_payload(), new_p->payload_len());
                    if (bytes_written < 0){
                        perror("Error: unable to write data to file\n");
                    }
                    buffered_p.erase(npb);
                    acknum += bytes_written % MAXSEQNUM ;
                    npb++;
                }
            }

        }
        else{
            // We didn't receive the right packet

            // Save it in a buffer, sort the buffer
            buffered_p.push_back(*new_p);
            buffered_p.sort(seqnum_comp);

            // Send the same ack as before, mark item as a duplicate packet
            Header ack_header;
            ack_header.acknum = acknum;
            ack_header.seqnum=seqnum;
            ack_header.dup = true; 
            int ack_len = HEADERSIZE;
            char* payload = "\0";
            writePacket(&ack_header, payload, ack_len, outgoing);
            sendto(sockfd, (const char *)outgoing, MSS, 0, (const struct sockaddr *)c_addr, sizeof(*c_addr));
        }
    }
} */

    