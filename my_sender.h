#ifndef MY_SENDER_H
#define MY_SENDER_H


#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
// #define DEBUG

#define and &&
// The first packet's sequence number is 1
// The last packet's sequene number is num_of_packet
#define BUFFERSIZE 1024

// 128 * 1.5KB = 1.5Mbps
//wind size = 128 1.5Mbps
// now 3Mbps
// minimum thres 

//1500-20-8 = 1472 B payload
//for safety reason, set MTU to 1450
#define MTU 1450
#define HEADER 28 // 20B IP + 8B UDP
#define MSS (MTU-HEADER)

#define CWND_INIT_SIZE 4
#define SSTHRESH_INIT_SIZE 32

#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2
#define HAND_SHAKE_STATE 3

//40000 is 60MB
#define MAX_POSSIBLE_PACKET 70000
#define INIT_SEQ_NUM UINT_MAX
#define HANDSHAKE_CONTENT  "****HAND*SHAKE****"



typedef unsigned int uint;

// we only need the sequence number to identify a packet
typedef struct my_pack {
    uint seq_number;
    char payload[MSS];
} my_packet;
// for acking a acket, we need to 
typedef struct my_ack {
     uint seqNum;
     uint ackNum;
} ack_packet;

char logbuffer[BUFFERSIZE];
void debug_print(const char *msg);
char* copy_to_payload(char* file_buffer, uint seq_numebr,uint total_packet_num, uint bytesToTransfer ,int *payload_size );
void setTimeOut(int length,int sockfd);
void send_packet(int sockfd, uint seq_num,char *payload, int payload_size);
char*  readFile(FILE *fp, unsigned long long bytesToTransfer);
ack_packet* recv_ack(int sockfd);
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename,uint bytesToTransfer);

#endif