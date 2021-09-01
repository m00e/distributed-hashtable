#ifndef _PEER_STRUCT
#define _PEER_STRUCT

#include <stdio.h>
#include <stdlib.h>

#define MAX_ID 65536 // 16-bit for the ID in the chord ring

typedef struct peer {
    struct peer *pred;
    struct peer *succ;
    uint16_t node_id;
    uint32_t node_ip;
    uint16_t node_port;
    
    fingertable *ft;
} peer;

int update_peer(peer *self, uint16_t new_node_id, uint32_t new_node_ip, uint16_t new_node_port, int flag);
int update_peer_succ(peer *self, uint16_t new_node_id, uint32_t new_node_ip, uint16_t new_node_port);
int update_peer_pred(peer *self, uint16_t new_node_id, uint32_t new_node_ip, uint16_t new_node_port);
peer *create_peer(int argc, const char **argv);
void print_peer_info(peer *self);
void free_peer(peer *self);

#endif
