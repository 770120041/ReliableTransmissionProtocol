#include "my_receiver.h"

struct sockaddr_in addr_mine, addr_other;

void debug_print(const char *msg){
    #ifdef DEBUG
        printf("%s\n",msg);
    #endif
}


void bind_socket(int sockfd,unsigned short int myUDPport){
    struct sockaddr_in addr_mine;
    memset((char *) &addr_mine, 0, sizeof (addr_mine));
    addr_mine.sin_family = AF_INET;
    // remember to trans to network format 
    addr_mine.sin_port = htons(myUDPport);
    addr_mine.sin_addr.s_addr = htonl(INADDR_ANY);
    //binding the socket
    if (bind(sockfd, (struct sockaddr*) &addr_mine, sizeof (addr_mine)) == -1){
        perror("bind port failed:");
        exit(1);
    }
    sprintf(logbuffer,"Binding to port %u\n",myUDPport);
    debug_print(logbuffer);
}


// get payload and seqence number to my packet
my_packet* recv_packet(int sockfd){
    char *recv_buffer = (char *) malloc(sizeof(my_packet)+100);
    memset(recv_buffer,0,sizeof(recv_buffer));
    socklen_t len_addr = sizeof(addr_other);
    ssize_t count = recvfrom(sockfd, recv_buffer, sizeof(my_packet), 0,(struct sockaddr*)&addr_other,&len_addr);
    
    if (count==-1) {
        perror("recv failed:");
        exit(-1);
    }

    my_packet* cur_packet = ( my_packet*) malloc(sizeof(my_packet));
    //remember to set to 0
    // memset(cur_packet,0,sizeof(cur_packet));
    //extract seq number
    memcpy(&(cur_packet->seq_number), recv_buffer, sizeof(uint));
    
    memcpy(cur_packet->payload, recv_buffer + sizeof(uint), MSS);
    //set the last bit to '\0' for convenience
    cur_packet->payload[MSS] = 0;


    //remember to free the buffer
    free(recv_buffer);

    return cur_packet;
}

void send_ack(int sockfd,uint ack_seq_number,uint ack_number){
    my_ack cur_ack;
    cur_ack.seqNum = ack_seq_number;
    cur_ack.ackNum = ack_number;
    ssize_t bytes_sent = sendto(sockfd, &cur_ack, sizeof(my_ack), 0, (struct sockaddr *) &addr_other, sizeof(addr_other));
    if(bytes_sent == -1){
        perror("send ack failed:");
        exit(-1);
    }
}

//store out of order packet to hash table
//refer here https://linux.die.net/man/3/hsearch
void store_packet_to_hash(my_packet *cur_packet){
    ENTRY e, *ep;
    char* hash_key = (char*)malloc(HASH_KEY_SIZE);
    memset(hash_key,0,sizeof(hash_key));
    int i = sprintf(hash_key, "%u", cur_packet->seq_number);
    hash_key[i] = 0;
    e.key = strdup(hash_key);
    e.data = (void *)cur_packet;
    ep  = hsearch(e, ENTER);
    if(ep  == NULL){
        perror("insert to hash failed:");
        exit(-1);
    }
    free(hash_key);
}

ENTRY* search_packet_from_hash(uint target_num ){
    ENTRY e;
    char* hash_key = (char*)malloc(HASH_KEY_SIZE);
    memset(hash_key,0,sizeof(hash_key));
    int i = sprintf(hash_key, "%u", target_num);
    hash_key[i] = 0;
    e.key = strdup(hash_key);
    free(hash_key);
    return  hsearch(e, FIND);
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
   
    // open file for output
    FILE *fout;
	if ((fout = fopen(destinationFile, "w+")) == NULL) {
		perror("fopen:");
        exit(-1);
	}

    int sockfd;
    //using UDP create SOCKET
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket:");
        exit(1);
    }
    bind_socket(sockfd,myUDPport);

    // There are two ways to terminate a connection
    // One is receiving a flag from sender, the other is detect the size we needed
    // I think detect the size and thus detect the numBytes received would be easier
    // total payload is received from inital packet

    


    uint  total_bytes_number= 0 ;
    uint total_packets_number=0;

    // the expected packet is cur_ack_number+1
    uint cur_ack_number = 0;
    uint total_bytes_write = 0;
    

    //handshake here
    my_packet* init_packet;
    int handshake = 1;
    while(handshake == 1){
       
        // the packet carries the total bytes to sent
        init_packet = recv_packet(sockfd);
        // sprintf(logbuffer,"in handshake, payload length is %s !");
        // debug_print(logbuffer);
        debug_print("in handshake");
        if(strcmp(init_packet->payload,HANDSHAKE_CONTENT) == 0){
            debug_print("reveiving handshake!");
            total_bytes_number = init_packet->seq_number;
            send_ack(sockfd,INIT_SEQ_NUM,0);
            free(init_packet);
        }
        else{
            handshake = 2;

        }
    }


    // free(init_packet);

    // send_ack(sockfd,INIT_SEQ,0);






    if(total_bytes_number % MSS == 0){
        //all full sized packets
        total_packets_number = (unsigned long long) total_bytes_number/MSS ;
    } else {
         //one more packets left
        total_packets_number = (unsigned long long) total_bytes_number/MSS +1; 
    }

    sprintf(logbuffer,"total_bytes_number %u,total number of packets %d",total_bytes_number,total_packets_number); 
    debug_print(logbuffer);
   
    

    //creating hash table
    if(!hcreate(70000)){
        perror("hash created failed:");
        exit(-1);
    }
    
    // the next sending seqence number of ACK, we send one ack for each inorder packets
    // it is useless actually 
    uint next_ack_seq_number = 0;
    while (total_bytes_write < total_bytes_number) {
        my_packet* cur_packet;
        if(handshake == 2){
            cur_packet = init_packet;
            handshake = 0;
        }
        else{
            cur_packet= recv_packet(sockfd);
        } 
        uint cur_sequence_number = cur_packet->seq_number ;
        // sprintf(logbuffer,"recv packet #%u",cur_sequence_number);
        // debug_print(logbuffer);

        next_ack_seq_number = cur_sequence_number;
        //packets already acknowledged, sending accumulative ack again
        if(cur_sequence_number < cur_ack_number+1){
            sprintf(logbuffer,"Recv packet #%u already acknowledged",cur_sequence_number);
            debug_print(logbuffer);
            free(cur_packet);
        }
        // out of order packets, store its
        else if(cur_sequence_number > cur_ack_number+1){
            sprintf(logbuffer,"Recv packet #%u already acknowledged,expected %u",cur_sequence_number,cur_ack_number+1);
            debug_print(logbuffer);
            store_packet_to_hash(cur_packet);
            //don't free packet here, because we have store it to hashtable
        }
        // inorder packets, store it
        else{
            debug_print(logbuffer);
            if (cur_sequence_number < total_packets_number) {
                fwrite(cur_packet->payload , MSS, 1, fout); //write result to file
                total_bytes_write += MSS;
                sprintf(logbuffer,"Writing  packet seq_numer %u,total bytes written:%u",cur_sequence_number,total_bytes_write);
            
            }
            //write last packets
            else{
                uint bytes_left = total_bytes_number % MSS;
                if(bytes_left == 0) bytes_left = MSS;
                fwrite(cur_packet->payload, bytes_left, 1, fout);
                total_bytes_write += bytes_left;
                sprintf(logbuffer,"Writing last packet seq_numer %u,size %d, total bytes written:%u",cur_sequence_number,bytes_left,total_bytes_write);
            
            }
            debug_print(logbuffer);
            
            cur_ack_number+=1;
            free(cur_packet);
        }

        ENTRY *e;
        while((e = search_packet_from_hash(cur_ack_number+1)) != NULL){

            my_packet * inorder_packet = (my_packet*) e->data; // get packet struct from hash table
            if(inorder_packet == NULL) {
                // Break here to make sure all written are inorder
                break;
            }

            if (inorder_packet->seq_number < total_packets_number) {
                fwrite(inorder_packet->payload, MSS, 1, fout); // write data to file
                total_bytes_write += MSS;
                
                sprintf(logbuffer,"Writing  packet seq_number #%u, size: %u",inorder_packet->seq_number,MSS);
                debug_print(logbuffer);
            } else if(inorder_packet->seq_number == total_packets_number) {
                uint bytes_left = total_bytes_number % MSS;
                if(bytes_left == 0) bytes_left = MSS;

                fwrite(inorder_packet->payload, bytes_left, 1, fout);
                total_bytes_write += bytes_left;

                sprintf(logbuffer,"Writing last packet seq_number #%u, size: %u",inorder_packet->seq_number,bytes_left);
                debug_print(logbuffer);
                //sending many ack for last packet
                send_ack(sockfd,next_ack_seq_number,cur_ack_number+1);
                send_ack(sockfd,next_ack_seq_number,cur_ack_number+1);
                send_ack(sockfd,next_ack_seq_number,cur_ack_number+1);
            }

            free(inorder_packet);
            cur_ack_number++;
        }


        // Attention: It will sent immediate ack for out of order packets
        send_ack(sockfd,next_ack_seq_number,cur_ack_number);
    }
    
    sprintf(logbuffer,"Writing %u,total %u",total_bytes_write,total_bytes_number);
    debug_print(logbuffer);
    //Don't need to send last packet here
    debug_print("receiving completed, program terminated");
    hdestroy();
   
    fclose(fout);
    close(sockfd);
    return;
}

int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    // UDP port
    udpPort = (unsigned short int) atoi(argv[1]);
    // path to write files
    reliablyReceive(udpPort, argv[2]);
}

