#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    //int recv_len;
    struct packet ack_pkt;

    int bytes_recv;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt

    struct pollfd pfds[1];
    pfds[0].fd = listen_sockfd;
    pfds[0].events = POLLIN;

    printf("listening");


    while(1){
        if((bytes_recv = recv(listen_sockfd, &buffer, sizeof(buffer)-1, 0)) == -1) {
            printf("error");
            break;
        }
        //received a packet -> send an ack

        //check if correct seq has been received
        if(expected_seq_num == buffer.seqnum){          // expected seq num came
            expected_seq_num = (expected_seq_num + buffer.length) % 40000;
            build_packet(&ack_pkt, 0, expected_seq_num, buffer.last, 1, 0, 0);  // build ack packet
            if(sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to)) < 0){
                perror("ack send error");
            }
            printRecv(&buffer);
            printSend(&ack_pkt, 0);
            fwrite(buffer.payload, buffer.length, 1, fp);   //write payload to output.txt
        } else {                                        // repeat packet received
            if(sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to)) < 0){
                perror("ack send error");
            }
            printRecv(&buffer);
            printSend(&ack_pkt, 1);
        }

        if(buffer.last){            // preparing to close down server bc Last flag received
            //keep open in case of another receive
            int events = poll(pfds, 1, 5000);       // keep server open for a while to check if client is still sending packets
            if (events == 0){                       // no packets received -> assumed client closed -> close server
                printf("finished packets");
                break;
            }                                       // else retransmit last ack packet
        }
        
    }
    

    printf("closes");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
