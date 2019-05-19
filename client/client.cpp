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

#define PORT     5100 
#define BUFFERSIZE 5240
#define MSS 524

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
    // Loading the file we want to send to server
    std::ifstream fin (fname, std::ifstream::binary);
    if (fin){
        fin.read(buffer,MSS);
        // add EOF 
        buffer[MSS] = '\0';
    }
    else {
        perror("ERROR: file not exists");
        exit(1);
    }
    int len;      
    sendto(sockfd, (const char *)buffer, MSS, 
        MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
            sizeof(servaddr)); 
    std::cout << "Send file!" << std::endl;
    close(sockfd); 
    return 0; 
} 