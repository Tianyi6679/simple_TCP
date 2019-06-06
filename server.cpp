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
  while (1){
    /* three-way-handshake */  
    cnct_server(sockfd, buffer, &h, payload, &c_addr, &len);
    connected = true;
    fname = std::to_string(++fileID) + ".file";
    fout.open(fname);
    std::cout << "Packet Received" << std::endl;
    fout << payload;

    while ( (pret = poll(ufd, 1, RTO)) > 0){
        if (recvfrom(sockfd,(char *)buffer, BUFFERSIZE, MSG_WAITALL, 
            (struct sockaddr*) &c_addr, &len) != -1){
            readPacket(&h, payload, PAYLOAD, buffer);
            logging(RECV, &h, 0, 0);
            /************************************************************************/
            if (getFIN(h.flags)){
                //wait_cls(2);
                cls_resp1(sockfd, buffer, &h, payload, &c_addr, 2);
                cls_resp2(sockfd, buffer, &h, payload, &c_addr, 2);
                connected = false;
                break;
            }
                /* TODO add file receiving logic */
                /* TODO add check dup logic */
            else{
                std::cout << "Packet Received" << std::endl;
                fout << payload;   
            }
            /**********************FILE RECEIVING STARTS HERE************************/

            // Create the buffer we'll read into and send over socket
            char incoming[MSS];
            memset(incoming, 0, MSS);
            char outgoing[MSS];
            memset(outgoing, 0, MSS);

            //Initialize some constants
            int recv_bytes = 0;
            Packet* recv_p = NULL;
            std::list<Packet> buffered_p;
            uint16_t acknum = (h.acknum + 1) % MAXSEQNUM;
            uint16_t seqnum = (h.seqnum + 1) % MAXSEQNUM;

            // While data is received
            //while(int count = recvfrom(sockfd, (char*) incoming, MSS, MSG_WAITALL, c_addr, &addr_len) != 0){
            //if (count = -1){
              //perror("Error reading from the socket");
              //exit(-1);
            //}
            // Else store the packet in an object
            recv_p = reinterpret_cast<Packet*>(buffer);
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
              sendto(sockfd, (const char *)outgoing, MSS, 0, (const struct sockaddr *)&c_addr, sizeof(c_addr));

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
              sendto(sockfd, (const char *)outgoing, MSS, 0, (const struct sockaddr *)&c_addr, sizeof(c_addr));
            }
      }
}
            /**********************FILE RECEIVING ENDS HERE**************************/
            /************************************************************************/
        }
    if (pret == -1) perror("polling failed");
    else if (pret == 0 & connected == true){
        std::cout<<"Timeout occurred! close connection!"<<std::endl;
        cls_init(sockfd, buffer, &h, payload, &c_addr, &len);
    }
    fout.close();
    std::cout << "Done!" << std::endl;
    connected = false;
    std::cout << "Close Connection to Client!" << std::endl;
  }
  close(sockfd);
  return 0;
}