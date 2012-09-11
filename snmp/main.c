
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "net.h"
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

static void log_message(SNMPMessage *message, char *sender_host)
{
    char *host_str = sender_host;
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

unsigned int send_request(Options *options, int socket, char *oid)
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

    send_udp_datagram(buf, len, socket, options->agent_host, options->agent_port);
    
    free(buf);
    
    return request_id;
}
        
static void check_requests(Options *options, int socket)
{
    ConfigItem *item = options->config->item_list;
    
    while (item != NULL)
    {
        item->wait--;
        
        if (item->wait <= 0)
        {
            send_request(options, socket, item->oid);
            item->wait = item->frequency;
        }
        
        item = item->next;
    }
}

static void check_for_responses(Options *options, int socket)
{
    while (1)
    {
        char buf[BUFLEN];
        SNMPMessage *message;
        char *sender_host;
        int sender_port;
        
        int nr = receive_udp_datagram(buf, BUFLEN, socket, &sender_host, &sender_port);
        
        if (nr == 0)
            break;
        
        if (options->verbose)
            fprintf(stderr, "Received packet from %s:%d\n", 
                    sender_host, sender_port);
        
        message = snmp_parse_message(buf, nr);
        
        if (options->verbose)
            snmp_print_message(message, stderr);
        
        if (snmp_get_pdu_type(message) == SNMP_GET_RESPONSE_TYPE)
            log_message(message, sender_host);
        
        snmp_destroy_message(message);
    }
}

static void run(Options *options)
{
    int socket = open_udp_socket(options->listen_port);

    if (options->verbose)
        fprintf(stderr, "Opened socket on port %d\n", options->listen_port);
    
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
        
        check_requests(options, socket);
        check_for_responses(options, socket);
        sleep(1);
    }
    
    close(socket);
}

int main(int argc, char *argv[])
{
    Options options;
    
    parse_args(argc, argv, &options);
    run(&options);
    return 0;
}
