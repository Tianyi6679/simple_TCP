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
    char padding[4];
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
        m_ssthresh = std::max((int)((float)m_cwnd/(float)2), ssthresh_base);
        m_cwnd = m_ssthresh + 1536;
    }

    void fast_retransmit_end(){
        m_cwnd = m_ssthresh;
        m_mode = 1;
    }

    void fast_recovery(){
        m_ssthresh = std::max((int)((float)m_cwnd/(float)2), ssthresh_base);
        m_cwnd = m_ssthresh + 1536;
        m_mode = 2;
    }

    int get_cwnd() const{
        return m_cwnd;
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
                m_cwnd += (int)((float)(512*512) / (float)m_cwnd);
            }
        }
        else if (m_mode == 1){
            m_cwnd += (int)((float)(512*512) / (float)m_cwnd);
        }
        else{
            m_cwnd += 512;
        }

    }

private:
    int m_cwnd;
    int max_cwnd = 10240;
    int m_ssthresh;
    int m_mode; // 0 for slow start, 1 for congestion avoidance, 2 for fast recovery
    int ssthresh_base = 1024;
    int cwnd_base = cwnd_base;
};

class Packet {
public:
  Packet(uint16_t dest_port, uint16_t seqnum, uint16_t acknum, uint16_t dup, char* data, int len, uint8_t flags) {
    header.dest_port = dest_port;
    header.seqnum = seqnum;
    header.acknum = acknum;
    header.dup = dup;
    header.flags = flags;
    memset(payload, 0, MSS);
    if (len > MSS) {
      fprintf(stderr, "Error creating packet: Payload larger than MSS.\n");
      exit(-1);
    } else {
      header.len = len;
    }
    if (payload  != NULL) {
      memcpy(payload, data, len);
    } else {
      fprintf(stderr, "Payload is empty\n");
      exit(-1);
    }
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
  
private:
  Header header;
  char payload[MSS-HEADERSIZE];
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
int cnct_server(int, char*, struct Header*, char*, struct sockaddr_in*, socklen_t*);
int cnct_client(int, char*, struct Header*, char*, struct sockaddr_in*, socklen_t*, int);
int cls_init(int, char*, struct Header*, char*, struct sockaddr_in*, socklen_t*);
int cls_resp1(int, char*, struct Header*, char*, struct sockaddr_in*, uint16_t);
int cls_resp2(int, char*, struct Header*, char*, struct sockaddr_in*, uint16_t);
int wait_cls(int);
int readPacket(struct Header* h, char* payload, int l, char* packet);
int writePacket(struct Header* h, char* payload, int l, char* packet);
#endif