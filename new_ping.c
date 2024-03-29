#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8

#define SOURCE_IP "10.0.2.0"
#define DESTINATION_IP "8.8.8.8"


// Checksum calculate
unsigned short calculate_checksum(unsigned short *paddress, int len);
// Create icmp packet and returns the size of the packet
int packetCreate(char *packet, int seq);
// Check that were passed 10 seconds till the next ping

void timeCheck() {
    struct timeval start, end;
   long time;
   gettimeofday(&start, NULL);
   while(1) {
        gettimeofday(&end, NULL);
        time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
        if(time >= 10000) {
            break;
        }
   }
   printf("Time was passed\n");
}
// Checksum calculate
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    
    sum = (sum >> 16) + (sum & 0xffff); 
    sum += (sum >> 16);                 
    answer = ~sum;                      

    return answer;
}

// Creat packet
int packetCreate(char *packet, int seq) {

    struct icmp icmphdr; // ICMP header
    char data[IP_MAXPACKET] = "This is the ping. \n";
    int datalen = strlen(data) + 1;
    
    // Message type (8 bits): echo request
    icmphdr.icmp_type = ICMP_ECHO;
    // Message code (8 bits): echo request
    icmphdr.icmp_code = 0;
    // Identifier (16 bits): some number to trace the response
    icmphdr.icmp_id = 18;
    // Sequence number (16 bits)
    icmphdr.icmp_seq = seq;
    // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
    icmphdr.icmp_cksum = 0;

    // Put the ICMP header in the packet
    memcpy((packet), &icmphdr, ICMP_HDRLEN);
    // Put the ICMP data in the packet
    memcpy(packet + ICMP_HDRLEN, data, datalen);
    // Calculate the ICMP header checksum
    icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    return ICMP_HDRLEN + datalen;
}

int main() {

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET; // IPv4
    dest_in.sin_addr.s_addr = inet_addr(DESTINATION_IP);

    // Create raw socket for IP-RAW
    int sock = -1;
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if(sock == -1) {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }
    

    int countSeq = 0;
    char packet[IP_MAXPACKET];

    char *args[2];
    // Compiled watchdog.c
    args[0] = "./watchdog";
    args[1] = NULL;

    while (1) {
        int pid = fork();
        if(pid == 0) {
            printf("Server %s can't be reached\n", DESTINATION_IP);
            timeCheck();
            kill(0, SIGKILL);
        }
        else {
        // Create packet
        int packetLen = packetCreate(packet, countSeq);
        
        
        struct timeval start, end;
        gettimeofday(&start, 0);

        // Send the packet
        int bytes_sent = sendto(sock, packet, packetLen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
        if (bytes_sent == -1){
            fprintf(stderr, "sendto() failed with error: %d\n", errno);
            return -1;
        }
        printf("Successfully sent one packet \n");

        // Get the ping response
        bzero(packet, IP_MAXPACKET);
        socklen_t len = sizeof(dest_in);
        ssize_t bytes_received = -1;
        while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len))) {
            if (bytes_received > 0) {
                // Check the IP header
                struct iphdr *iphdr = (struct iphdr *)packet;
                struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
                break;
            }
        }
        gettimeofday(&end, 0);
        kill(pid, SIGKILL);

        // Get reply data from the packet
        char reply[IP_MAXPACKET];
        memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, packetLen - ICMP_HDRLEN);


        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
        float time = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        printf("   %d bytes from %s: seq: %d time: %0.3fms\n", bytes_received, DESTINATION_IP, countSeq, time);

        countSeq++;
        bzero(packet, IP_MAXPACKET);
        sleep(1);
        }

    } 
    // Close the raw socket 
    close(sock);
    return 0;
}