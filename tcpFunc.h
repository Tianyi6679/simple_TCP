#ifndef TCPFUNC_H
#define TCPFUNC_H
#include <stdint.h>
#include <stdio.h>
#include <chrono>
#include <ctime>
#include <cstring>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <algorithm>
#include <iostream>
#define BUFFERSIZE 5240
#define HEADERSIZE 12
#define MSS 524
#define MAXSEQNUM 25600
#define RECV 1
#define SEND 0
#define PAYLOAD 512
#define RTO 10000
#define WaitCLS 2000
#define RET_TO 500
/* TODO change either cwnd or dest_port (won't use them at all) to indicate duplicate */
struct Header{
    uint16_t dest_port;
    uint16_t seqnum;
    uint16_t acknum;   
    /*
    FLAGS: ACK|FIN|SYN|PAD|PAD|PAD|PAD|PAD
    */
    uint8_t flags;
    uint16_t dup;
    uint16_t len;
    //char padding[1];
};

class Timer
{
public:
    void start()
    {
        m_start = std::chrono::steady_clock::now();
        m_running = true;
    }
    
    void stop()
    {
        m_end = std::chrono::steady_clock::now();
        m_running = false;
    }
    
    double elapsed()
    {
        std::chrono::time_point<std::chrono::steady_clock> endTime;
        
        if(m_running)
        {
            endTime = std::chrono::steady_clock::now();
        }
        else
        {
            endTime = m_end;
        }
        
        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_start).count();
    }
    
private:
    std::chrono::time_point<std::chrono::steady_clock> m_start;
    std::chrono::time_point<std::chrono::steady_clock> m_end;
    bool m_running = false;
};

class CongestionControl {
public:
    CongestionControl(){
        m_cwnd = cwnd_base;
        m_ssthresh = ssthresh_base;
        m_mode = 0;
    }

    void change_mode(int newmode){
        m_mode = newmode;
    }

    void timeout(){
        m_ssthresh = std::max((int)((float)m_cwnd/(float)2), ssthresh_base);
        m_cwnd = cwnd_base;
        change_mode(0);
    }

    void fast_retransmit_start(){
        std::cout << "3 DUP ACK received, starting fast retransmit \n";
        m_ssthresh = std::max((int)((float)m_cwnd/(float)2), ssthresh_base);
        m_cwnd = m_ssthresh + 1536;
    }

    void fast_retransmit_end(){
        std::cout << "All packets in range have been resent, entering CA\n";
        m_cwnd = m_ssthresh;
        m_mode = 1;
    }

    void fast_recovery(){
        m_ssthresh = std::max((int)((float)m_cwnd/(float)2), ssthresh_base);
        m_cwnd = m_ssthresh + 1536;
        m_mode = 2;
    }

    int get_cwnd() const{
        return m_cwnd/512 * 512;
    }

    int get_ssthresh() const{
        return m_ssthresh;
    }

    int get_mode() const{
        return m_mode;
    }

    void update(){
        if (m_mode == 0){
            if (m_cwnd < m_ssthresh){
                m_cwnd += 512;
            }
            else{
                change_mode(1);
                m_cwnd += (512*512) / m_cwnd;
            }
        }
        else if (m_mode == 1){
            m_cwnd += (512*512) / m_cwnd;
        }
        else{
            m_cwnd += 512;
        }
        if (m_cwnd > max_cwnd){
          m_cwnd = max_cwnd;
        }
    }

private:
    int m_cwnd;
    int max_cwnd = 10240; // 20 * 512
    int m_ssthresh;
    int m_mode; // 0 for slow start, 1 for congestion avoidance, 2 for fast recovery
    int ssthresh_base = 5120;
    int cwnd_base = PAYLOAD;
};

class Packet {
public:
  Packet(struct Header* in_h, char* in_payload){
    memset(payload, 0, PAYLOAD);
    memcpy(payload, in_payload, in_h->len);
    this->header.dest_port = in_h->dest_port;
    this->header.seqnum = in_h->seqnum;
    this->header.acknum = in_h->acknum;
    this->header.dup = in_h->dup;
    this->header.flags = in_h->flags;
    this->header.len = in_h->len;
  }
  bool valid_seq() const {
    return (header.seqnum >= 0 && header.seqnum <= MAXSEQNUM);
  }
  bool valid_ack() const {
    return (header.seqnum >= 0 && header.seqnum <= MAXSEQNUM);
  }
  int payload_len() const {
    return header.len;
  }
  int packet_size() const {
    return HEADERSIZE + header.len;
  }
  int h_seqnum() const {
    return header.seqnum;
  }
  int h_acknum() const {
    return header.acknum;
  }
  uint8_t h_flags() const{
    return header.flags;
  }
  short h_dup() const {
    return header.dup;
  }
  char* p_payload() {
    return payload;
  }
  Header p_header() const {
    return header;
  }
  void printPack() const {
    std::cout<<header.seqnum<<' '<<header.len<<std::endl;
    std::cout<<(int)header.flags<<std::endl;
  }
private:
  struct Header header;
  char payload[PAYLOAD];
};


bool getACK(uint8_t flags);
bool getFIN(uint8_t flags);
bool getSYN(uint8_t flags);
void setACK(uint8_t* flags);
void setFIN(uint8_t* flags);
void setSYN(uint8_t* flags);
void resetFLAG(uint8_t* flags);
void initConn(struct Header*h);
int logging(int, struct Header*, int cwnd, int ssthresh);
bool seqnum_comp(Packet a, Packet b);
int cnct_server(int, char*, struct Header*, char*, struct sockaddr_in*, socklen_t*);
int cnct_client(int, char*, struct Header*, char*, struct sockaddr_in*, socklen_t*, int, CongestionControl);
int cls_init(int, char*, struct Header*, char*, struct sockaddr_in*, socklen_t*, uint16_t);
int cls_resp1(int, char*, struct Header*, char*, struct sockaddr_in*, uint16_t);
int cls_resp2(int, char*, struct Header*, char*, struct sockaddr_in*, uint16_t);
int wait_cls(int);
int readPacket(struct Header* h, char* payload, int l, char* packet);
int writePacket(struct Header* h, char* payload, int l, char* packet);
#endif