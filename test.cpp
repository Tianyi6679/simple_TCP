#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <string.h>
#include <iostream>
#include <csignal>
static std::ofstream fout;
void signalHandler(int signum){
    if (fout.is_open()){
        fout.close();
        fout.open("2.file", std::ofstream::trunc);
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
int main(){
    /*
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
    */
    signal(SIGQUIT, signalHandler);
    signal(SIGTERM, signalHandler);
    fout.open("2.file");
    fout<< "write something";
    fout.flush();
    sleep(1000);
}