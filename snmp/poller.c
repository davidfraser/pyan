
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include "snmp.h"

#define BUFLEN 65535

#define DEFAULT_LISTEN_PORT 12345

#define DEFAULT_AGENT_PORT 161

typedef struct Options
{
    int verbose;
    int listen_port;
    char *agent_host;
    int agent_port;
} Options;

void diep(char *s)
{
    perror(s);
    exit(1);
}

static int split_host_port(char *input, int default_port, char **host, int *port)
{
    char *p;
    
    *host = strdup(input);
    
    if ((p = strchr(*host, ':')))
    {
        *port = strtol(p+1, NULL, 0);
        *p = 0;
    }
    else
    {
        *port = default_port;
    }
    
    return 1;
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
    
    if (optind >= argc)
    {
        fprintf(stderr, "Need an agent host\n");
        exit(1);
    }
    else if (!split_host_port(argv[optind], DEFAULT_AGENT_PORT, &options->agent_host, &options->agent_port))
    {
        fprintf(stderr, "Agent host cannot be parsed\n");
        exit(1);
    }
    else if (options->agent_port < 1 || options->agent_port > 65535)
    {
        fprintf(stderr, "Agent port must be between 1 and 65535\n");
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

static unsigned int next_request_id = 0;

unsigned int send_request(Options *options, int s, struct sockaddr *target, int target_len)
{
    SNMPMessage *message;
    int len;
    unsigned char *buf;
    unsigned long int request_id = next_request_id++;
  
    message = snmp_create_message();
    snmp_set_version(message, 0);
    snmp_set_community(message, "public");
    snmp_set_pdu_type(message, SNMP_GET_REQUEST_TYPE);
    snmp_set_request_id(message, request_id);
    snmp_set_error(message, 0);
    snmp_set_error_index(message, 0);
    snmp_add_varbind_null(message, "1.3.6.1.2.1.1.5.0");
    
    len = snmp_message_length(message);
    buf = malloc(len);
    snmp_render_message(message, buf);
    
    if (options->verbose)
        snmp_print_message(message, stderr);
    
    snmp_destroy_message(message);
    
    if (options->verbose)
        fprintf(stderr, "Sending datagram to %s:%d\n", options->agent_host, options->agent_port);
    
    if (sendto(s, buf, len, 0, target, target_len) == -1)
        diep("sendto");
    
    free(buf);
    
    return request_id;
}

static void check_for_responses(Options *options, int s)
{
    while (1)
    {
        struct sockaddr_in si_other;
        socklen_t slen = sizeof(si_other);
        char buf[BUFLEN];
        SNMPMessage *message;
        int nr;
        
        nr = recvfrom(s, buf, BUFLEN, MSG_DONTWAIT, (struct sockaddr *) &si_other, &slen);
        if (nr == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            
            diep("recvfrom");
        }
        
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
}

static void run(Options *options)
{
    struct sockaddr_in si_me, si_other;
    int s;
    socklen_t slen = sizeof(si_other);
    int reuse = 1;
    struct hostent *he;
    
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0)
        diep("setsockopt");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(options->listen_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *) &si_me, sizeof(si_me)) != 0)
        diep("bind");

    if (options->verbose)
        fprintf(stderr, "Opened socket on port %d\n", options->listen_port);

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(options->agent_port);
    
    if (!(he = gethostbyname2(options->agent_host, AF_INET)))
        diep("gethostbyname2");
    
    memmove(&si_other.sin_addr.s_addr, he->h_addr, he->h_length);
    
    while (1)
    {
        int i;
        send_request(options, s, (struct sockaddr *) &si_other, slen);
        for (i = 0; i < 5; i++)
        {
            check_for_responses(options, s);
            sleep(1);
        }
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
