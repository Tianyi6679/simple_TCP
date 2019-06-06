#include <stdio.h> 
#include <stdlib.h> 
#include <list>
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/poll.h>
#include <fstream>
#include <iostream>
#include "../tcpFunc.h"
#define PORT     5100
int main(int argvc, char** argv) { 
    int sockfd; 
    char buffer[BUFFERSIZE]; 
    memset(buffer, sizeof(buffer), '\0');
    struct sockaddr_in     servaddr; 
    char* fname = argv[1];
    // Creating socket file descriptor 
    
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("ERROR: socket construction failed"); 
        exit(1); 
    } 
    
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    socklen_t len;
    struct Header h;
    char payload[MSS-sizeof(struct Header)];
    memset(payload, '\0', sizeof(payload));
    // Def congestion manager
    CongestionControl congestion_manager;
    // Establish connection to server
    cnct_client(sockfd, buffer, &h, payload, &servaddr, &len, PORT, congestion_manager);
    /************************************************************************/
                        /* example file sending logic begins*/
    std::ifstream fin (fname, std::ios::binary | std::ios::ate);
    // Get the file length
    int filelen = fin.tellg();
    // Reset the pointer to the beginning of the file to read from it
    fin.seekg(0, std::ios::beg);
    if (fin){
        fin.read(payload, MSS);
    }
    else {
        perror("ERROR: file not exists");
        exit(1);
    }
    h.acknum = h.seqnum + 1;
    h.seqnum = MSS;
    h.flags = 0;
    setACK(&(h.flags));
    writePacket(&h, payload, PAYLOAD, buffer);
    send(sockfd, (const char *)buffer, MSS, 0); 
    logging(SEND, &h, 0, 0);

    // Initialize buffer we'll read into and send over the socket
    char resp_buffer[MSS];
    memset(resp_buffer, 0, MSS);
    // And the one for the packet payload
    char p_buff[PAYLOAD];
    memset(p_buff, 0, PAYLOAD);
    // And the one we'll read from
    char recv_buff[BUFFERSIZE];
    memset(recv_buff, 0, BUFFERSIZE);
    // for debugging
    bool debug = true;
    //wait_cls(2);

    // Initialize timer, seqnum, acknum, congestion control, ack counter
    Timer rto;
    uint16_t seqnum = h.seqnum;
    uint16_t acknum = h.acknum;
    uint16_t cwnd = MSS;
    std::list<Packet> unacked_p;
    int bytes_read = 0;

    // Monitor socket for input
    struct pollfd s_poll[1];
    s_poll[0].fd = sockfd;
    s_poll[0].events = POLLIN;

    // Until we're done reading
    while (1){
        if (debug){
            std::cout << "Reading!\n";
            }
        while(bytes_read < congestion_manager.get_cwnd()){
            fin.read(p_buff, PAYLOAD);
            // How many bytes did we actually read?
            std::streamsize count = fin.gcount();
            if (debug){
                char bytes_read[128];
                sprintf(bytes_read, "Bytes Read: %ld\n", count);
                std::cout << bytes_read << std::endl;
            }
            if (count == 0){
                if (debug){
                std::cout << "Done reading!\n";
                }
                fin.close();
                memset(resp_buffer, 0, MSS);
                break;
            }
            // Prepare Packet for Sending - add to list
            if (unacked_p.empty()){
                if (debug){
                std::cout << "Initializing List!";
                } 
                Packet p(h.dest_port, 0, 0, 0, resp_buffer, count, 0);
                unacked_p.push_back(p);
            }
            else{
                if (debug){
                std::cout << "Packet list is not empty !";
                } 
                Packet last_packet_added = unacked_p.back();
                uint16_t last_seqnum = last_packet_added.h_seqnum();
                uint16_t last_acknum = last_packet_added.h_acknum();
                Packet p(h.dest_port, last_seqnum+1, last_acknum+1, cwnd, resp_buffer, count, 0);
                unacked_p.push_back(p);
            }
            // Update seqnum
            bytes_read += count;
        }

        // Send all the unsent packets

        std::list<Packet>::iterator packet_iter = unacked_p.begin();
        while(packet_iter!=unacked_p.end()){
            if (packet_iter->h_seqnum() > seqnum){
                Header out_header = packet_iter->p_header();
                char* out_payload = packet_iter->p_payload();
                int out_len = packet_iter->payload_len();
                writePacket(&out_header, out_payload, out_len, resp_buffer);
                seqnum += MSS;
                packet_iter++;
            }
        }
        int sock_event = 0;

        if (sock_event = poll(s_poll, 1, RTO) > 0){
            if (s_poll[0].revents & POLLIN){
                /*Receive Packet*/
                int recv_count;
                Header recv_h;
                char recv_p[MSS];
                if (recv_count = recvfrom(sockfd, (char *)recv_buff, BUFFERSIZE, MSG_WAITALL, 
                    (struct sockaddr*) &servaddr, &len) != -1){
                        readPacket(&recv_h, recv_p, 0, recv_p);
                        logging(RECV, &recv_h, congestion_manager.get_cwnd(), congestion_manager.get_ssthresh());
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
    //std::cout << "Send File!" << std::endl;
                        /* file sending logic ends */


                        
    /************************************************************************/
    cls_init(sockfd, buffer, &h, payload, &servaddr, &len);
    
    /* handle close request from server, put it into sending logic */
    #if 0
    if (recv(sockfd,(char *)buffer, BUFFERSIZE, MSG_WAITALL) != -1){
        readPacket(&h, payload, 0, buffer);
        (RECV, &h, 0, 0);
        if (getFIN(h.flags)){
            cls_resp1(sockfd, buffer, &h, payload, &servaddr, 2);
            cls_resp2(sockfd, buffer, &h, payload, &servaddr, 2);
        }
    }
    #endif
    //std::cout<< "Closing Connection to Server" << std::endl;
    close(sockfd); 
    return 0; 
} 