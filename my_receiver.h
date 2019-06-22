#ifndef MY_RECEIVER_H
#define MY_RECEIVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <search.h>
#include <limits.h>
#include <errno.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
// #define DEBUG


#define BUFFERSIZE 2048


//1500-20-8 = 1472 B payload
// WE SET 1450 for safety reason
#define MTU 1450
#define HEADER 28 // 20B IP + 8B UDP
#define MSS (MTU-HEADER)
#define INIT_SEQ_NUM UINT_MAX
#define HANDSHAKE_CONTENT "****HAND*SHAKE****"
#define END_SEQ UINT_MAX-1
#define HASH_KEY_SIZE 10






typedef unsigned int uint;

char logbuffer[BUFFERSIZE];
// we only need the sequence number to identify a packet
typedef struct my_pack {
    uint seq_number;
    char payload[MSS+1];
} my_packet;
// for acking a acket, we need to ack its seq and accumalative ack number
typedef struct ack_packet {
     uint seqNum;
     uint ackNum;
} my_ack;

void debug_print(const char *msg);
void bind_socket(int sockfd,unsigned short int myUDPport);

// get payload and seqence number to my packet
my_packet* recv_packet(int sockfd);

void send_ack(int sockfd,uint ack_seq_number,uint ack_number);

void store_packet_to_hash(my_packet *cur_packet);

ENTRY* search_packet_from_hash(uint target_num );

void reliablyReceive(unsigned short int myUDPport, char* destinationFile);


#endif