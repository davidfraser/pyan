
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
#include <time.h>

#include "snmp.h"
#include "config.h"

#define BUFLEN 65535

#define DEFAULT_LISTEN_PORT 12345

#define DEFAULT_AGENT_PORT 161

#define DEFAULT_CONFIG_FILENAME "sample.conf"

typedef struct Options
{
    int verbose;
    int listen_port;
    char *agent_host;
    int agent_port;
    char *config_filename;
    Config *config;
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
    
    options->config_filename = DEFAULT_CONFIG_FILENAME;
    options->config = NULL;
}

void get_time_str(char *buf, int size)
{
    time_t time_buf;
    struct tm tm_buf;
    
    time(&time_buf);
    localtime_r(&time_buf, &tm_buf);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_buf);    
}

static void log_message(SNMPMessage *message, struct sockaddr_in *sender, int sender_len)
{
    char *host_str = inet_ntoa(sender->sin_addr);
    char timestamp_str[20];
    char *oid_str;
    char *value_str;
    int i = 0;
    
    get_time_str(timestamp_str, sizeof(timestamp_str));
    
    while (snmp_get_varbind(message, i, &oid_str, &value_str))
    {
        printf("%s\t%s\t%s\t%s\n", host_str, timestamp_str, oid_str, value_str);
        i++;
    }
}

static unsigned int next_request_id = 0;

unsigned int send_request(Options *options, int s, struct sockaddr *target, int target_len, char *oid)
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
    snmp_add_varbind_null(message, oid);;
    
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
        
static void check_requests(Options *options, int s, struct sockaddr *target, int target_len)
{
    ConfigItem *item = options->config->item_list;
    
    while (item != NULL)
    {
        item->wait--;
        
        if (item->wait <= 0)
        {
            send_request(options, s, target, target_len, item->oid);
            item->wait = item->frequency;
        }
        
        item = item->next;
    }
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
            log_message(message, &si_other, slen);
        
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
        if (options->config == NULL)
        {
            options->config = load_config(options->config_filename);
            if (options->verbose)
            {
                fprintf(stderr, "Loading config from %s\n", options->config_filename);
                print_config(options->config, stderr);
            }
        }
        
        check_requests(options, s, (struct sockaddr *) &si_other, slen);
        check_for_responses(options, s);
        sleep(1);
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
