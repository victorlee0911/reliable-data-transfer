#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv_start;
    struct timeval tv_curr;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
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

    //reading in file
    fseek(fp, 0L, SEEK_END);
    long int file_size = ftell(fp);
    char *file_content = (char*)malloc(file_size+1);
    fseek(fp, 0, SEEK_SET);

    int total_packets = (file_size / (PAYLOAD_SIZE-1)) + 1;
    int base = 0;
    int next_pkt = 0;
    int cwind = WINDOW_SIZE;
    long int bits_sent = 0;

    while(1){                         //while there is more to read

        if(base + cwind > total_packets){
            cwind = total_packets - base;
        }
        printf("nxtpkt: %d \n base: %d \n cwind: %d \n", next_pkt, base, cwind);
        if (next_pkt < base + cwind){
            int bytes_send = PAYLOAD_SIZE-1;
            if(bits_sent + bytes_send >= file_size){          //last packet sends less bytes
                bytes_send = file_size - bits_sent;
                last = 1;
            }
            fseek(fp, next_pkt * (PAYLOAD_SIZE-1), SEEK_SET);
            fread(file_content, bytes_send, 1, fp);
            if(last){
                file_content[bytes_send] = '\0';
            }
            build_packet(&pkt, seq_num, ack_num, last, ack, bytes_send, file_content);
            if(sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to)) < 0){
                perror("send error");
            }
            bits_sent += bytes_send;
            seq_num = (bits_sent) % 40000;                          //updating sequence number
            printSend(&pkt, 0);
            printf("%d", total_packets);
            if(base == next_pkt){
                //start_timer
                gettimeofday(&tv_start, 0);
            }
            next_pkt += 1;
        }
        
        int events = poll(pfds, 1, 50);
        while(events != 0){
            if(events == -1){
                perror("poll error");
            }
            if((recv(listen_sockfd, &ack_pkt, sizeof(ack_pkt)-1, 0)) == -1) {       
                printf("error");
                break;
            }
            printRecv(&ack_pkt);

            if(ack_pkt.last) {
                fclose(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return 0;
            }

            if(ack_pkt.acknum >= base * (PAYLOAD_SIZE - 1)){
                base = (ack_pkt.acknum / (PAYLOAD_SIZE - 1));
                printf("updated base: %d", base);
            }
            if(next_pkt < base){
                next_pkt = base;
            }
            gettimeofday(&tv_start, 0);
            events = poll(pfds, 1, 0);
            
            // if(ack_pkt.acknum != seq_num){
            //     printf("wrong ack received");
            // }
        }

        gettimeofday(&tv_curr, 0);
        long milliseconds = (tv_curr.tv_sec - tv_start.tv_sec) * 1000L;
        milliseconds += (tv_curr.tv_usec - tv_start.tv_usec) / 1000L;

        if(milliseconds >= 300){    //timeout
           bits_sent = base*(PAYLOAD_SIZE - 1);
           seq_num = bits_sent % 40000;
           next_pkt = base;
        }
        usleep(40000);
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
    }
 
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

