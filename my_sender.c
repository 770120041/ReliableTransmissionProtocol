#include "my_sender.h"

// variables for congestion control
uint cwnd_start = 1; // Packet sequence starts with 1
uint cwnd_size = CWND_INIT_SIZE;
uint dup_acks = 0;
//slow start threshold size, ssthresh
uint ssthresh = SSTHRESH_INIT_SIZE;

uint MINIMUM_THRES =  16;
uint MAX_WINDOW_SIZE = 128;
int state = SLOW_START; 
uint max2(uint a, uint b){
    if(a>b) return a;
    return b;
}
void packet_loss_handling(){
  
}

struct sockaddr_in addr_other;

void debug_print(const char *msg){
    #ifdef DEBUG
        printf("%s\n",msg);
    #endif
}



char* copy_to_payload(char* file_buffer, uint seq_numebr,uint total_packet_num,uint bytesToTransfer,int *payload_size ){
    char* tmp_payload = (char*)malloc(MSS+1);
    
    if (seq_numebr == total_packet_num) {
        *payload_size = bytesToTransfer % MSS;;
        if(*payload_size==0) {
            *payload_size = MSS;
        }
        memcpy(tmp_payload, file_buffer+((seq_numebr-1)*MSS), *payload_size);
        tmp_payload[*payload_size] = 0;
    } else if (seq_numebr < total_packet_num) {
        memcpy(tmp_payload, file_buffer+(((seq_numebr-1))*MSS), MSS);
        tmp_payload[MSS] = 0;
        *payload_size = MSS;
    }
    return tmp_payload;
}



void setTimeOut(int length,int sockfd){
    struct timeval InterVal;
    InterVal.tv_sec = 0;
    //from us to ms is 10^3
    InterVal.tv_usec = length*1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
        perror("so_reuseaddr:");
        exit(-1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &InterVal, sizeof(struct timeval)) < 0) {
        perror("setsock:");
        exit(-1);
    }
    // close returns immediately  TODO: if we need to check the last was received
    struct linger lin;
    lin.l_onoff = 0;
    lin.l_linger = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(int));

}

// Send packet with given buffer and seq_numebr
void send_packet(int sockfd, uint seq_num,char *payload, int payload_size) {
    my_packet *cur_packet = (my_packet*)malloc(sizeof(my_packet)); 

    memset(cur_packet, 0, sizeof(my_packet));
    cur_packet->seq_number = seq_num;
    memcpy(cur_packet->payload,payload,payload_size);
    sprintf(logbuffer,"sending packet seq %u,payload %u",seq_num,payload_size);
    debug_print(logbuffer);
    if(sendto(sockfd, cur_packet, sizeof(my_packet), 0, (struct sockaddr *) &addr_other, sizeof(addr_other)) == -1) {
        perror("sendto:");
        exit(-1);
    }
    free(payload);
}

// Read in files to buffer one batch a time, cause the buffer is not large enough to hold the whole file
// Read in the whole file
char*  readFile(FILE *fp, unsigned long long bytesToTransfer) {

    fseek (fp, 0, SEEK_END);
    unsigned long long  file_size = ftell(fp);
    
    fseek (fp, 0, SEEK_SET);

    //if file size smaller, then we can only read file size length
    unsigned long long bytes_to_read = bytesToTransfer;
    // if(file_size < bytesToTransfer ){
    //     bytes_to_read = file_size;
    // }
    char* file_buffer =  (char*)malloc(file_size);
    if (file_buffer)
    {
        fread (file_buffer, 1, file_size, fp);
    }

    return file_buffer;
}

ack_packet* recv_ack(int sockfd){
    ack_packet* cur_ack = (ack_packet*)malloc(sizeof(ack_packet));
    memset(cur_ack, 0, sizeof(ack_packet));
    socklen_t len_other = sizeof(addr_other);
    if(recvfrom(sockfd, cur_ack, sizeof(ack_packet), 0, (struct sockaddr *) &addr_other, &len_other) == -1){
        debug_print("recv ack timeout");
        return NULL;   
    }
    return cur_ack;
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, uint bytesToTransfer) {
   
   //for large file, big min windows
    if(bytesToTransfer > 50485760 && bytesToTransfer < 100485760) {
        MINIMUM_THRES = 1900;
        MAX_WINDOW_SIZE = 4096;
        debug_print("large, adjust minimum thres");
    }
   // open the file that is going to be sent
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr,"Can't open flle %s",filename);
        exit(1);
    }

    int sockfd;

    //get UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket:");
        exit(-1);
    }

    //get other addr
    memset((char *) &addr_other, 0, sizeof (addr_other));
    addr_other.sin_family = AF_INET;
    addr_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &addr_other.sin_addr) == 0) {
        perror("inet_aton:");
        exit(1);
    }

    
    
    // As the sliding window in TCP, we set timeout to be 2 RTT
    // our RTT is 20ms, so we can set 1.5 times RTT as  timeout
    setTimeOut(30,sockfd);


    
    uint total_packet_num = 0;
     // Calculate the pktsToTransfer
    if (bytesToTransfer % MSS == 0) {
        total_packet_num = bytesToTransfer / MSS; // the total # packets to transfer
    } else {
        total_packet_num = bytesToTransfer / MSS + 1;
    }

    // Read the required bytes
    char* file_buffer = readFile(fp,bytesToTransfer);

    //send init packet
    sprintf(logbuffer,"total_bytes_number  %u",bytesToTransfer); 
    debug_print(logbuffer);



    // HANDSHAKE HERE
    state = HAND_SHAKE_STATE;
    while(state == HAND_SHAKE_STATE){
        char* tmp_payload = malloc(BUFFERSIZE);
        strcpy(tmp_payload,HANDSHAKE_CONTENT);
        sprintf(logbuffer,"pyalod size   %d",strlen(HANDSHAKE_CONTENT)); 
        debug_print(logbuffer);
        send_packet(sockfd,bytesToTransfer,tmp_payload,strlen(HANDSHAKE_CONTENT));
        ack_packet* init_ack = recv_ack(sockfd);
        if(!init_ack){
            debug_print("Handshake failed: try again!");
            continue;
        }
        if(init_ack->seqNum == INIT_SEQ_NUM){
            debug_print("Handshake succeed!");
            state = SLOW_START;
        }
    }
   
    // sprintf(tmp_payload,"%u",bytesToTransfer);
   
   
    // free(tmp_payload);


    // ack_packet* init_ack = recv_ack(sockfd);
    // if(!init_ack){
    //     debug_print("init ack failed: try again!");
    //     init_ack = recv_ack(sockfd);
    //     if(!init_ack){
    //         perror("init ack:");
    //         exit(-1);
    //     }
    // }

    // // if(init_ack){
    // //     debug_print("connection establised! Begin transmitting:");
    // // }
    // if(init_ack->seqNum == INIT_SEQ_NUM){
    //     debug_print("connection establised! Begin transmitting:");
    // }
      
    // Using slow start and congestion avoidance for stable connection
    // Using fast recovery for equal speed
    // refer from https://tools.ietf.org/html/rfc2001

/*
    1.  Initialization for a given connection sets cwnd to one segment
    
    2.  The TCP output routine never sends more than the minimum of cwnd
       and the receiver's advertised window.

    3.  When congestion occurs (indicated by a timeout or the reception
       of duplicate ACKs), one-half of the current window size (the
       minimum of cwnd and the receiver's advertised window, but at
       least two segments) is saved in ssthresh.  Additionally, if the
       congestion is indicated by a timeout, cwnd is set to one segment
       (i.e., slow start).

   4.  When new data is acknowledged by the other end, increase cwnd,
       but the way it increases depends on whether TCP is performing
       slow start or congestion avoidance.
*/
    
    // num of packets that the receiver acked
    uint total_ack_number = 0;

    uint accumulative_increase = 0;
    // an array to record packets being acked, 1 is acked, 0 is not acked
    int *is_acked = malloc(sizeof(int) *(MAX_POSSIBLE_PACKET));
    for(int i=0;i<MAX_POSSIBLE_PACKET;i++) is_acked[i] = 0;

    // Sending packets by sliding window
    // Using an array to store if the receiver has acked a packet
    // We adjust the window size depending on the ack we received
    int timout_cnt = 0;

    while(total_ack_number < total_packet_num){


        //the upper bound for sequnce number of this round
        //This is an upper bound that shouldn't be reached
        // the seq_numbe shall be always smaller than it
        if(cwnd_size>MAX_WINDOW_SIZE){
            cwnd_size = MAX_WINDOW_SIZE;
        }
        uint seq_upper_bound = cwnd_start + cwnd_size;
        if (seq_upper_bound >= total_packet_num + 1) {
            seq_upper_bound = total_packet_num + 1;
        }

        
        
        uint packets_sent_this_batch = 0;
        uint ack_recv_this_batch = 0;

        // the first packet of this batch
        uint cur_seq_number = cwnd_start;
        //send packets
        while (cur_seq_number < seq_upper_bound) {
            //send packets that are not acked
            if(is_acked[cur_seq_number] == 0){
                int payload_size;
                char* tmpPayload = copy_to_payload(file_buffer,cur_seq_number,total_packet_num,bytesToTransfer,&payload_size);
                send_packet(sockfd,cur_seq_number,tmpPayload,payload_size);
                packets_sent_this_batch++;
            }
            cur_seq_number++;
        }
        sprintf(logbuffer,"[%d]cwnd_size = %u,cwnd start = %u,packets_sent_this_batch=%u",state,cwnd_size,cwnd_start,packets_sent_this_batch);
        debug_print(logbuffer);








        
        //check if all packets are being received sent this batch or all packets are acked
        while (ack_recv_this_batch < packets_sent_this_batch && total_ack_number < seq_upper_bound ) {
            // if(cur_seq_number == seq_upper_bound-1){
            //     //all packets are sent
            //     break;
            // }
            // sprintf(logbuffer,"ack_recv_this_batch = %u,packets_sent_this_batch = %u,total_ack_number=%u,seq_upper_bound=%u ",\
            // ack_recv_this_batch,packets_sent_this_batch,total_ack_number,seq_upper_bound);
            // debug_print(logbuffer);

            ack_packet* cur_ack = recv_ack(sockfd);
            

            //recv timeout
            // update cwnd_window, mark unack packets, go back to sennding again

            if(!cur_ack){
                timout_cnt++;
                if(timout_cnt == 20){
                    debug_print("keeping on timeout, they may have close the transmission");
                    perror("keeping on timeout, they may have close the transmission");
                    exit(-1);
                }
                // if(state == SLOW_START){
                ssthresh = cwnd_size/2;
                if(ssthresh < MINIMUM_THRES) ssthresh = MINIMUM_THRES;
                cwnd_size = ssthresh;
                // }
                
                // else{
                //     cwnd_size = max2(ssthresh + 3,MINIMUM_THRES);
                // }
                // packet_loss_handling();
                // if (state == FAST_RECOVERY) {
                //     ssthresh  = cwnd_size/2;
                //     if(ssthresh<128) ssthresh = 128;
                //     cwnd_size = ssthresh+3;
                // } else {
                //     cwnd_size = 4;
                // }
                dup_acks = 0;


                sprintf(logbuffer,"packet #%u timeout, ssthresh is set to %u,cwnd set to %u",total_ack_number,ssthresh,cwnd_size);
                debug_print(logbuffer);
                int payload_size;
                char* tmp_payload = copy_to_payload(file_buffer,total_ack_number+1,total_packet_num,bytesToTransfer,&payload_size);
                send_packet(sockfd,total_ack_number+1,tmp_payload,payload_size);
                break;
            }


            timout_cnt = 0;

            if(cur_ack->seqNum == INIT_SEQ_NUM){
                debug_print("handshake packet, continnue");
                continue;
            }

            uint receiver_ack_number =cur_ack->ackNum;
            // fast recovery reference:https://www.isi.edu/nsnam/DIRECTED_RESEARCH/DR_WANIDA/DR/JavisInActionFastRecoveryFrame.html
            // equals to ack number
            if(receiver_ack_number == total_ack_number){
                //  //resend packet for duplicate ack
                // char* tmp_payload = copy_to_payload(file_buffer,total_ack_number+1,total_packet_num,bytesToTransfer);
                // send_packet(sockfd,total_ack_number+1,tmp_payload,strlen(tmp_payload));
                 // Receive duplicate Ack
                if (state == SLOW_START) {
                    dup_acks ++;
                    if(dup_acks == 3){
                        ssthresh = cwnd_size/2;
                        if(ssthresh < 2) ssthresh = 2;
                        cwnd_size = max2(cwnd_size, ssthresh +3);
                        sprintf(logbuffer,"[%d]Recv dup acks,expect #%u , ssthresh is set to %u,cwnd set to %u",state,receiver_ack_number+1,ssthresh,cwnd_size);
                        debug_print(logbuffer);
                        //fast recovery handle packet loss
                        state = FAST_RECOVERY;
                       
                    }
                }
                else if (state == FAST_RECOVERY ) {
                    if(cwnd_size < MAX_WINDOW_SIZE ) cwnd_size++;
                    sprintf(logbuffer,"[%d]Recv dup acks,expect #%u , ssthresh is set to %u,cwnd set to %u",state,receiver_ack_number+1,ssthresh,cwnd_size);
                    debug_print(logbuffer);
                } 
                else if(state == CONGESTION_AVOIDANCE){
                   dup_acks++;
                   if(dup_acks == 3){
                        state = FAST_RECOVERY;
                        ssthresh = cwnd_size/2;
                        if(ssthresh <2) ssthresh = 2;
                        cwnd_size = max2(ssthresh+3,cwnd_size);
                        sprintf(logbuffer,"[%d]ecv dup acks,expect #%u , ssthresh is set to %u,cwnd set to %u",state,receiver_ack_number+1,ssthresh,cwnd_size);
                        debug_print(logbuffer);
                   }
                }
            }
            //less than ack number, don't care
            else if(receiver_ack_number < total_ack_number){
                
            }
            // larger than cur ack number,packet sending out of order
            else{
                sprintf(logbuffer,"[%d]receiver ack #%u ,our ack %u",state,receiver_ack_number,total_ack_number);
                debug_print(logbuffer);
                dup_acks  = 0;

                 // Receive new Ack
                if (state ==SLOW_START ) {
                    if(cwnd_size < MAX_WINDOW_SIZE){
                        cwnd_size += (receiver_ack_number-total_ack_number);
                    }
                    if(cwnd_size>ssthresh){
                        state = CONGESTION_AVOIDANCE;
                    }
                } else if (state == FAST_RECOVERY) {
                    state = CONGESTION_AVOIDANCE;
                    cwnd_size = max2(ssthresh+3,MINIMUM_THRES);
                   
                } else if (state == CONGESTION_AVOIDANCE) {
                    accumulative_increase += 1;
                    // increase slowly
                    if(accumulative_increase % cwnd_size == 0){
                        cwnd_size += 1;
                    }
                    // Increase winSize with 1/winSize
                }
               

                //slide the window 
                if( receiver_ack_number > total_ack_number){
                   
                    for(int i=total_ack_number;i<=cwnd_start;i++){
                        is_acked[i] = 1;
                    }
                    total_ack_number = receiver_ack_number;
                    cwnd_start = total_ack_number+1;
                    
                }
               
                
            }
            

            ack_recv_this_batch++;
        }
       
    }

    
    free(file_buffer);
    fclose(fp);
    close(sockfd);
    debug_print("sending complete, program terminated");

}

int main(int argc, char** argv) {

    unsigned short int udpPort;
    uint numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage:%s ip port filename bytes\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);


    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (0);
}