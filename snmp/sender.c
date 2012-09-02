
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include "snmp.h"

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
}

void run(Options *options)
{
    struct sockaddr_in si_me, si_other;
    int s;
    socklen_t slen = sizeof(si_other);
    int reuse = 1;
    struct hostent *he;
    unsigned int next_request_id = 0;
    
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
        SNMPMessage *message;
        int len;
        unsigned char *buf;
      
        message = snmp_create_message();
        snmp_set_version(message, 0);
        snmp_set_community(message, "public");
        snmp_set_pdu_type(message, SNMP_GET_REQUEST_TYPE);
        snmp_set_request_id(message, next_request_id++);
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
        
        if (sendto(s, buf, len, 0, (struct sockaddr *) &si_other, slen) == -1)
            diep("sendto");
        
        free(buf);
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
