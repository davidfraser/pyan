#ifndef NET_H
#define NET_H

int split_host_port(char *input, int default_port, char **host, int *port);

int open_udp_socket(int port);

void send_udp_datagram(void *buf, int len, int socket, char *target_host, int target_port);

int receive_udp_datagram(void *buf, int max, int socket, char **sender_host, int *sender_port);

#endif
