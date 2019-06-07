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
    char payload[PAYLOAD];
    std::cout<<sizeof(payload)<<' '<<sizeof(struct Header)<<std::endl;
    
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
        fin.read(payload, PAYLOAD);
    }
    else {
        perror("ERROR: file not exists");
        exit(1);
    }

    h.acknum = h.seqnum + 1;
    h.seqnum = 1;
    h.flags = 0;
    std::streamsize count = fin.gcount();
    h.len = count;
    setACK(&(h.flags));
    writePacket(&h, payload, PAYLOAD, buffer);
    send(sockfd, (const char *)buffer, MSS, 0); 
    logging(SEND, &h, congestion_manager.get_cwnd(), congestion_manager.get_ssthresh());

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
    uint16_t seqnum = (h.seqnum + count) % MAXSEQNUM;
    uint16_t cwnd = PAYLOAD;
    std::list<Packet> unacked_p;
    int bytes_read = 0;
    bool reach_eof = false;

    // Add first packet to list
    if (debug){
    std::cout << "Initializing List!\n";
    } 
    Packet p(&h, payload);
    unacked_p.push_back(p);
    bytes_read = count;

    // Monitor socket for input
    struct pollfd s_poll[1];
    s_poll[0].fd = sockfd;
    s_poll[0].events = POLLIN;
    int no_dup = 0;

    // Until we're done reading
    bool first_packet = true;
    while (!reach_eof){
        if (debug){
            std::cout << "Reading!\n";
        }
        
        if (!first_packet && !unacked_p.empty()){
            // Send all the unsent packets
            std::list<Packet>::iterator packet_iter = unacked_p.begin();
            while(packet_iter != unacked_p.end()){
                if (packet_iter->h_seqnum() <= seqnum){
                    Header out_header = packet_iter->p_header();
                    char* out_payload = packet_iter->p_payload();
                    int out_len = packet_iter->payload_len();
                    writePacket(&out_header, out_payload, out_len, resp_buffer);
                    send(sockfd, (const char *)resp_buffer, MSS, 0); 
                    logging(SEND, &out_header, congestion_manager.get_cwnd(), congestion_manager.get_ssthresh());
                }
                packet_iter++;
            }
        }
        else first_packet = false;
        
        while(bytes_read < congestion_manager.get_cwnd()){
            std::cout<<congestion_manager.get_cwnd()<<std::endl;
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
                reach_eof = true;
                break;
                // handle connection teardown
            }
            struct Header new_header;
            // Update seqnum
            new_header.seqnum = seqnum;
            seqnum = (seqnum +count) % MAXSEQNUM;
            new_header.acknum = 0;
            new_header.len = count;
            resetFLAG(&(new_header.flags));
            Packet p(&new_header, p_buff);
            unacked_p.push_back(p);
            bytes_read += count;
            p.printPack();
            writePacket(&new_header, p_buff, count, resp_buffer);
            send(sockfd, (const char *)resp_buffer, MSS, 0); 
            logging(SEND, &new_header, congestion_manager.get_cwnd(), congestion_manager.get_ssthresh());
            std::cout<<bytes_read<<std::endl;
        }
        wait_cls(1);
        int sock_event = 0;
        
        rto.start();
        int time_left = RET_TO;
        while ((sock_event = poll(s_poll, 1, time_left)) > 0){
            if (s_poll[0].revents & POLLIN){
                /*Receive Packet*/
                int recv_count;
                Header recv_h;
                //char recv_p[MSS];
                if (recv_count = recv(sockfd, (char *)recv_buff, BUFFERSIZE, 0) != -1){
                        readPacket(&recv_h, payload, 0, recv_buff);
                        logging(RECV, &recv_h, congestion_manager.get_cwnd(), congestion_manager.get_ssthresh());
                        //Is it a duplicate?
                        if (recv_h.dup){
                            std::cout << "Got a duplicate ACK! \n";
                            no_dup ++;
                            if (no_dup > 2){
                                congestion_manager.fast_retransmit_start();
                            }
                            break;
                        }
                        /*Were we expecting this ack num?*/
                        std::list<Packet>::iterator packet_iter = unacked_p.begin();
                        uint16_t recv_ack = recv_h.acknum;
                        //std::cout<<recv_ack<<std::endl;
                        while(packet_iter != unacked_p.end()){
                            //packet_iter->printPack();
                            uint16_t cur_ack = (packet_iter->h_seqnum() + packet_iter->payload_len()) % MAXSEQNUM;
                            //std::cout<< cur_ack<< std::endl;
                            if (cur_ack <= recv_ack){
                                // Clear packet from unreceived acks
                                bytes_read -= packet_iter->payload_len();
                                unacked_p.erase(packet_iter);
                                // Update ssthresh and cwnd
                                congestion_manager.update();
                                // Reset number of duplicates to 0
                                no_dup = 0;
                                if (cur_ack == recv_ack) break;
                            }
                            packet_iter++;
                            
                        }
                    }
                    // If list is empty, break
                    if(unacked_p.empty()){
                        break;
                    }
            }
            time_left = time_left - rto.elapsed();
            if (time_left <= 0){
                break;
            }
        }
        if (sock_event == 0 || time_left <= 0){
            congestion_manager.timeout();
        }
        if (debug){
          std::cout << resp_buffer << std::endl;
        }
    }

                        
    /************************************************************************/
    if (cls_init(sockfd, buffer, &h, payload, &servaddr, &len) == -1){
        perror("Connection abort!");
        exit(1);
    }
    
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
    close(sockfd); 
    return 0; 
} 