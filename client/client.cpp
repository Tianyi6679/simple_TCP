#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
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
    // Establish connection to server
    cnct_client(sockfd, buffer, &h, payload, &servaddr, &len, PORT);
    // Loading the file we want to send to server
    std::ifstream fin (fname, std::ifstream::binary);
    if (fin){
        fin.read(buffer, MSS);
        // add EOF 
        buffer[MSS] = '\0';
    }
    else {
        perror("ERROR: file not exists");
        exit(1);
    }
    h.acknum = h.seqnum + 1;
    h.seqnum = MSS;
    h.flags = 0;
    setACK(&(h.flags));
    send(sockfd, (const char *)buffer, MSS, 0); 
    std::cout << "Send File!" << std::endl;
    cls_init(sockfd, buffer, &h, payload, &servaddr, &len);
    std::cout<< "Client Close" << std::endl;
    close(sockfd); 
    return 0; 
} 