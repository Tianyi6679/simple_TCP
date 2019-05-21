#include "server.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include "header.h"
#include "packet.h"
#define BUFFERSIZE 5240
#define HEADERSIZE 12
#define MSS 524
int main(int argvc, char** argv)
{
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
  struct header h;
  char payload[MSS - sizeof(struct header)];
  socklen_t len = sizeof(struct sockaddr);
  while (1){
    /* Start processing the request */
    while (!connected){
        recvfrom(sockfd,(char *)buffer, MSS, MSG_WAITALL, (struct sockaddr*) &c_addr, &len);
        readPacket(&h, payload, 0, buffer);
        if (getSYN(h.flags)){
            std::cout<<"SYN received "<<ntohs(c_addr.sin_port)<<" "<<c_addr.sin_addr.s_addr<<std::endl;
            h.acknum = h.seqnum + 1;
            h.seqnum = 0;
            setACK(&(h.flags));
            connected = true;
        }    
        
    }
    writePacket(&h, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 0, (const struct sockaddr *) &c_addr, len);
    std::cout<<"Connection Established "<<c_addr.sin_port<<" "<<c_addr.sin_addr.s_addr<<std::endl;
    if (recvfrom(sockfd,(char *)buffer, BUFFERSIZE, MSG_WAITALL, 
        (struct sockaddr*) &c_addr, &len) != -1){
      std::cout << "packet received" << std::endl;
      std::ofstream fout("1.file");
      fout << buffer << std::endl;
      fout.close();
      std::cout << "Done!" << std::endl;
    }
    connected = false;
  }
  close(sockfd);
  return 0;
}
