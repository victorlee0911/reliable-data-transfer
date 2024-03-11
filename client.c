#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    long int seq_num = 0;
    long int ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server

    //struct to handle poll()
    struct pollfd pfds[1];
    pfds[0].fd = listen_sockfd;
    pfds[0].events = POLLIN;

    int packets = 0;
    long int base = 0;
    long int next_seq = 0;

    //reading in file
    fseek(fp, 0L, SEEK_END);
    long int file_size = ftell(fp);
    char *file_content = (char*)malloc(file_size+1);
    fseek(fp, 0, SEEK_SET);

    packets = file_size / PAYLOAD_SIZE;
    while(seq_num < file_size){                         //while there is more to read
        // int bytes_send = PAYLOAD_SIZE-1;
        // if(seq_num + bytes_send >= file_size){          //last packet sends less bytes
        //     bytes_send = file_size - seq_num;
        //     last = 1;
        // }
        // fread(file_content, bytes_send, 1, fp);
        // if(last){
        //     file_content[bytes_send] = '\0';
        // }
        // build_packet(&pkt, seq_num, ack_num, last, ack, bytes_send, file_content);
        // if(sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to)) < 0){
        //     perror("send error");
        // }
        // seq_num += bytes_send;                        //updating sequence number
        // printSend(&pkt, 0);
        // printf("%d", packets--);

        //TODO: REMOVE ABOVE
        //TODO: BELOW GO_BACK_N

         while(next_seq < base + WINDOW_SIZE){
            int bytes_send = PAYLOAD_SIZE-1;
            if(seq_num + bytes_send >= file_size){          //last packet sends less bytes
                bytes_send = file_size - seq_num;
                last = 1;
            }
            fread(file_content, bytes_send, 1, fp);
            if(last){
                file_content[bytes_send] = '\0';
            }
            seq_num += bytes_send;
            build_packet(&pkt, seq_num, ack_num, last, ack, bytes_send, file_content);
            if(sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to)) < 0){
                perror("send error");
            }
            
            if(base == next_seq){
                //start_timer
            }
            next_seq += 1;
         }

        if((recv(listen_sockfd, &ack_pkt, sizeof(ack_pkt)-1, 0))){
            if(ack_pkt.acknum > base * (PAYLOAD_SIZE - 1)){
                base = (ack_pkt.ackum / (PAYLOAD_SIZE - 1));
            }
            if(next_seq < base){
                next_seq = base;
            }
            if(base == next_seq){
                //stop_timer
            }
        }

        if(timeout){
           seq_num = (next_seq-base)*(PAYLOAD_SIZE - 1);
           next_seq = base;
        }
        //TODO: REMOVE BELOW
        // while(1){
        //     int events = poll(pfds, 1, 300);            //poll sleeps program until socket receives a packet or 400ms timeout triggers
        //     if(events == 0){                            //no packets received... aka timeout triggered
        //         //time out -> retransmit
        //         printSend(&pkt, 1);
        //         if(sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to)) < 0){      
        //             perror("send error");
        //         } 
        //     } else {
        //         break;
        //     }
        // }
        // if((recv(listen_sockfd, &ack_pkt, sizeof(ack_pkt)-1, 0)) == -1) {       
        //     printf("error");
        //     break;
        // }
        // printRecv(&ack_pkt);
        // if(ack_pkt.acknum != seq_num){
        //     printf("wrong ack received");
        // }
        //TODO: REMOVE ABOVE
    }
 
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

