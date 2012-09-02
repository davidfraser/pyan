
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>

#include "snmp.h"

#define BUFLEN 65535

#define DEFAULT_LISTEN_PORT 12345

typedef struct Options
{
    int verbose;
    int listen_port;
} Options;

void diep(char *s)
{
    perror(s);
    exit(1);
}

static void parse_args(int argc, char *argv[], Options *options)
{
    int c;
    
    options->verbose = 0;
    options->listen_port = DEFAULT_LISTEN_PORT;

    opterr = 1;

    while ((c = getopt (argc, argv, "vp:")) != -1)
        switch (c)
        {
            case 'v':
                options->verbose = 1;
                break;
            case 'p':
                options->listen_port = strtol(optarg, NULL, 0);
                break;
            default:
                exit(1);
        }

    if (options->listen_port < 1 || options->listen_port > 65535)
    {
        fprintf(stderr, "Listen port must be between 1 and 65535\n");
        exit(1);
    }
}

static void log_message(SNMPMessage *message)
{
    char *host_str = "host";
    char *timestamp_str = "timestamp";
    char *oid_str;
    char *value_str;
    int i = 0;
    
    while (snmp_get_varbind(message, i, &oid_str, &value_str))
    {
        printf("%s\t%s\t%s\t%s\n", host_str, timestamp_str, oid_str, value_str);
        i++;
    }
}

static void run(Options *options)
{
    struct sockaddr_in si_me, si_other;
    int s;
    socklen_t slen = sizeof(si_other);
    char buf[BUFLEN];
    int reuse = 1;

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0)
        diep("setsockopt");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(options->listen_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *) &si_me, sizeof(si_me)) == -1)
        diep("bind");
    
    if (options->verbose)
        fprintf(stderr, "Listening on port %d\n", options->listen_port);

    while (1)
    {
        SNMPMessage *message;
        int nr;
        
        nr = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen);
        if (nr == -1)
            diep("recvfrom");
        
        if (options->verbose)
            fprintf(stderr, "Received packet from %s:%d\n", 
                    inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        
        message = snmp_parse_message(buf, nr);
        
        if (options->verbose)
            snmp_print_message(message, stderr);
        
        if (snmp_get_pdu_type(message) == SNMP_GET_RESPONSE_TYPE)
            log_message(message);
        
        snmp_destroy_message(message);
    }

    close(s);
}

int main(int argc, char *argv[])
{
    Options options;
    
    parse_args(argc, argv, &options);
    run(&options);
    return 0;
}
