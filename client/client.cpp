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
#include "../header.h"
#include "../packet.h"
#define PORT     5100 
#define BUFFERSIZE 5240
#define MSS 524
void initConn(struct header*h){
    h->seqnum = 0;
    h->acknum = 0;
    h->flags = 0;
    h->dest_port = PORT;
    setSYN(&(h->flags));
    return;
}
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
    struct header h;
    char payload[MSS-sizeof(struct header)];
    memset(payload, '\0', sizeof(payload));
    // Establish connection to server
    bool connected = false;
    // step 1
    initConn(&h);
    writePacket(&h, payload, 0, buffer);
    sendto(sockfd, (const char *)buffer, MSS, 
        MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
            sizeof(servaddr)); 
    std::cout << "Send SYN" <<std::endl;
    // step 2       
    while(!connected){
        recvfrom(sockfd,(char *)buffer, MSS, 0, 
        (struct sockaddr*) &servaddr, &len);
        readPacket(&h, payload, 0, buffer);
        if (getSYN(h.flags) && getACK(h.flags) && h.acknum == 1){
            std::cout<<"ACK received"<<std::endl;
            connected = true;
        }
        else{
            initConn(&h);
            writePacket(&h, payload, 0, buffer);
            sendto(sockfd, (const char *)buffer, MSS, 
                    MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
                    sizeof(servaddr)); 
            std::cout << "Resend SYN" <<std::endl;
        }
    }
    // step 3
    std::cout<<"Connection Established"<<std::endl;
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1){
        perror("ERROR: connect failed");
        exit(1);
    }
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
    std::cout << "Send file!" << std::endl;
    close(sockfd); 
    return 0; 
} 