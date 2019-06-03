#include <iostream>
#include "header.h"
#include "packet.h"
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <string.h>
using namespace std;

int main(){
    char buffer[524];
    memset(buffer, '\0', sizeof(buffer));
    cout<<sizeof(buffer)<<endl;
    struct header h;
    memset(&h, 0, sizeof(struct header));
    char* hello = "Hello World";
    h.dest_port = 5100;
    h.seqnum = 0;
    h.acknum = 1;
    h.flags = 8;
    cout<<h.dest_port<<endl;
    
    struct header n;
    char newst[100];
    int size = writePacket(&h, hello, strlen(hello), buffer);
    cout<<size<<endl;
    readPacket(&n, newst, strlen(hello), buffer);
    cout<<n.dest_port<<endl;
    cout<<newst<<endl;
    
    uint8_t a = 240;
    uint8_t b = 160;
    uint8_t c = 0;
    cout<<getACK(a)<<' '<<getFIN(a)<<' '<<getSYN(a)<<endl;
    cout<<getACK(b)<<' '<<getFIN(b)<<' '<<getSYN(b)<<endl;
    setACK(&c);
    setFIN(&c);
    setSYN(&c);
    cout<<(int)c<<endl;;
    return 0;
}