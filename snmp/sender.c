
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "snmp.h"

#define SRV_IP "127.0.0.1"

#define NPACK 10
#define PORT 12345

void diep(char *s)
{
    perror(s);
    exit(1);
}

int main(void)
{
    struct sockaddr_in si_other;
    int s, i, slen = sizeof(si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    
    if (inet_aton(SRV_IP, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    for (i = 0; i < NPACK; i++)
    {
        SNMPMessage *message;
        int len;
        unsigned char *buf;
      
        printf("Generating message\n");
        
        message = snmp_create_message();
        snmp_set_version(message, 0);
        snmp_set_community(message, "private");
        snmp_set_pdu_type(message, SNMP_GET_REQUEST_TYPE);
        snmp_set_request_id(message, i);
        snmp_set_error(message, 0);
        snmp_set_error_index(message, 0);
        snmp_add_varbind_null(message, "1.3.6.1.4.1.2680.1.2.7.3.2.0");
        
        len = snmp_message_length(message);
        buf = malloc(len);
        snmp_render_message(message, buf);
        
        snmp_print_message(message);
        snmp_destroy_message(message);
        
        printf("Sending packet %d\n", i);
        if (sendto(s, buf, len, 0, &si_other, slen) == -1)
            diep("sendto()");
        
        free(buf);
    }

    close(s);
    return 0;
}
