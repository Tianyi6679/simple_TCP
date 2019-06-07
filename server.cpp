#include "server.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <cstdio>
#include <bits/stdc++.h>
#include <csignal>
#include <sys/poll.h>
#include "tcpFunc.h"

static std::ofstream fout;
static std::string fname;

void signalHandler(int signum){
    if (fout.is_open()){
        fout.close();
        fout.open(fname, std::ofstream::trunc);
        fout.write("INTERRUPT", 9);
        fout.close();
    }
    switch (signum){
        case SIGQUIT:
            std::cout<< " Interrupt signal SIGQUIT received." <<std::endl;
            signum = 0;
            break;
        case SIGTERM:
            std::cout<< " Interrupt signal SIGTERM received." <<std::endl;
            signum = 0;
            break;
        default:
            std::cout<< " Unexpected interrupt signal "<<signum<<" received." <<std::endl;
    }
    exit(signum);
}
int main(int argvc, char** argv)
{
    
  signal(SIGQUIT, signalHandler);
  signal(SIGTERM, signalHandler);
  /* Initialize a debugging flag */
  bool debug = false;

  /* Define port number */
  int port;
  if (argvc <= 1 ){
    port = MYPORT;
    if (debug){
      std::cout<<"Using default port: 5100"<<std::endl;
    }
  }
  else{
    port = atoi(argv[1]);
    if (debug){
      std::cout<<port<<std::endl;
    }
  }
  /* Define Sockets (one to listen, one to accept the incoming connection) */
  int sockfd, new_fd;

  /* Define structures to hold addresses */
  struct sockaddr_in my_addr;
  struct sockaddr_in c_addr;
  memset(&my_addr, 0, sizeof(my_addr));
  memset(&c_addr, 0, sizeof(c_addr));
  
  unsigned int sin_size;

  /* Define a buffer that will store incoming packets */
  char buffer[BUFFERSIZE];
  memset(buffer,sizeof(buffer),'\0');
  
  /* Initialize the socket, define its address and domain */
  if ((sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1){
    perror("ERROR: socket construction failed");
    exit(1);
  }
  
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  
  /* Assign a port to the socket */
  if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1){
    perror("ERROR: bind failed");
    exit(1);
  }

  if (debug){
    std::cout<<"bind\n";
  }
  bool connected = false;  
  struct Header h;
  char payload[MSS - sizeof(struct Header)];
  socklen_t len = sizeof(struct sockaddr);
  
  /* setup poll events */
  struct pollfd ufd[1];
  ufd[0].fd = sockfd;
  ufd[0].events = POLLIN;
  int pret;
  int fileID = 0;
  uint16_t seqnum = 0;
  uint16_t acknum = 0;
  while (1){
    /* three-way-handshake */  
    cnct_server(sockfd, buffer, &h, payload, &c_addr, &len);
    seqnum = 1;
    connected = true;
    fname = std::to_string(++fileID) + ".file";
    fout.open(fname, std::ios::binary | std::ios::ate | std::ios::trunc);
    //fout << "This is a test/n";
    //fout.flush();
    std::cout << "Packet Received" << std::endl;
    fout << payload;
    std::cout<<std::string(payload)<<std::endl;
    struct Header h_ack;
    resetFLAG(&(h_ack.flags));
    setACK(&(h_ack.flags));
    //std::cout<<sizeof(payload)<<' '<<h.seqnum<<std::endl;
    h_ack.acknum = (h.seqnum + sizeof(payload)) % MAXSEQNUM; 
    h_ack.seqnum = ++seqnum;
    writePacket(&h_ack, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 0, (const struct sockaddr *)&c_addr, sizeof(c_addr));
    logging(SEND, &h_ack, 0, 0);

    while ( (pret = poll(ufd, 1, RTO)) > 0){
        if (recvfrom(sockfd,(char *)buffer, BUFFERSIZE, MSG_WAITALL, 
            (struct sockaddr*) &c_addr, &len) != -1){
            readPacket(&h, payload, PAYLOAD, buffer);
            logging(RECV, &h, 0, 0);
            
            if (getFIN(h.flags)){
                //wait_cls(2);
                cls_resp1(sockfd, buffer, &h, payload, &c_addr, seqnum);
                cls_resp2(sockfd, buffer, &h, payload, &c_addr, seqnum);
                connected = false;
                break;
            }
            else{
                std::cout << "Packet Received" << std::endl; 
                //std::cout<<std::string(payload)<<std::endl;
                // Create the buffer we'll read into and send over socket
                
                char incoming[MSS];
                memset(incoming, 0, MSS);
                char outgoing[MSS];
                memset(outgoing, 0, MSS);

                //Initialize some constants
                int recv_bytes = 0;
                Packet* recv_p = NULL;
                std::list<Packet> buffered_p;
                
                // Else store the packet in an object
                
                Packet* new_p = new Packet(&h, NULL);

                // Is it the packet we expected?
                if (new_p->h_acknum() == acknum){
                  fout << payload;
                  std::cout<<std::string(payload)<<std::endl;
                  // Send ack for packet
                  Header ack_header;
                  resetFLAG(&(ack_header.flags));
                  setACK(&(ack_header.flags));
                  ack_header.acknum = (h.seqnum + sizeof(payload)) % MAXSEQNUM;
                  ack_header.seqnum = (++seqnum % MAXSEQNUM);
                  writePacket(&ack_header, payload, 0, outgoing);
                  sendto(sockfd, (const char *)outgoing, MSS, 0, (const struct sockaddr *)&c_addr, sizeof(c_addr));
                  logging(SEND, &ack_header, 0, 0);

                  // Do any of the buffered packets now fall in order?
                  std::list<Packet>::const_iterator npb = buffered_p.begin();
                  while(npb != buffered_p.end()){
                    while (!buffered_p.empty() && npb->h_acknum() == acknum){
                        fout << payload; 
                        buffered_p.erase(npb);
                        acknum += MSS % MAXSEQNUM ;
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
                  resetFLAG(&(ack_header.flags));
                  setACK(&(ack_header.flags));
                  ack_header.acknum = acknum;
                  ack_header.seqnum= seqnum;
                  ack_header.dup = (uint16_t)true; 
                  int ack_len = HEADERSIZE;
                  writePacket(&ack_header, payload, 0, outgoing);
                  sendto(sockfd, (const char *)outgoing, MSS, 0, (const struct sockaddr *)&c_addr, sizeof(c_addr));
                  logging(SEND, &ack_header, 0, 0);
                }
            }
        }
            /**********************FILE RECEIVING ENDS HERE**************************/
            /************************************************************************/
    }
    if (pret == -1) perror("polling failed");
    else if (pret == 0 & connected == true){
        std::cout<<"Timeout occurred! close connection!"<<std::endl;
        if (cls_init(sockfd, buffer, &h, payload, &c_addr, &len) == -1)
        {
            std::cout<<"Force Close Connection!"<<std::endl;
        }
    }
    fout.close();
    std::cout << "Done!" << std::endl;
    connected = false;
    std::cout << "Close Connection to Client!" << std::endl;
  }
  close(sockfd);
  return 0;
}