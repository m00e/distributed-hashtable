#ifndef BLOCK3_NET_H
#define BLOCK3_NET_H

#include <stdint.h> // for uint16_t, uint32_t
#include <stddef.h> // for size_t
#include "protocol.h" // for packet

int build_connection_as_client(const char* node, const char* port);
int build_connection_as_server(const char* port);
int _build_connection(const char* node, const char* port,  int isServer);
int accept_connection(int socketfd);
void* recvn(int socketfd, void* buf, size_t len, size_t* offset);
void* sendn(int socketfd, void* packet, size_t len, int flags);
void* recv_packet(int socketfd, packet* pkg, size_t* pkg_size);
int ask_next(void* lookup_pkg, const char* ip, const char* port);
int forward_packet(char* ip, char* port, void* pkg, size_t len);
char* ip4_to_string(uint32_t ip);
char* port_to_string(uint16_t port);

#endif // BLOCK3_NET_H
