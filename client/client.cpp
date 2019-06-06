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
    /************************************************************************/
                        /* example file sending logic begins*/
    std::ifstream fin (fname, std::ifstream::binary);
    if (fin){
        fin.read(payload, MSS);
        // add EOF 
        buffer[MSS] = '\0';
    }
    else {
        perror("ERROR: file not exists");
        exit(1);
    }
    //wait_cls(2);
    h.acknum = h.seqnum + 1;
    h.seqnum = MSS;
    h.flags = 0;
    setACK(&(h.flags));
    writePacket(&h, payload, PAYLOAD, buffer);
    send(sockfd, (const char *)buffer, MSS, 0); 
    logging(SEND, &h, 0, 0);
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