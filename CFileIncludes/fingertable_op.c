#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../Headers/fingertable_op.h"
#include "../Headers/protocol.h"

int power2(int n) {
    int sum = 1;
    for(int i = 0; i < n; i++) {
        sum *= 2;
    }
    return sum;
}

int get_ith_ftentry(int i, int k, int m) {
    return (k+power2(i)) % power2(m);
}

/* Return values:
 *      i >= 0 - Index of start val, where the value is located
 *      i = -1 - ERROR
 */
int get_index_from_value(uint16_t start_vals[FT_SIZE], uint16_t start_val) {
    int i = 0;
    for(;i < FT_SIZE; i++) {
        if(start_vals[i] == start_val) {
            return i;
        }
    }
    return -1;
}

void add_ft_entry__peer(fingertable *ft, peer *peer_to_add, int index) {
    ft->ids[index] = peer_to_add->node_id;
    ft->ips[index] = peer_to_add->node_ip;
    ft->ports[index] = peer_to_add->node_port;
    
    ft->size++;
    return;
}

void add_ft_entry__packet(fingertable *ft, packet req, int index) {
    ft->ids[index] = req.node_id;
    ft->ips[index] = req.node_ip;
    ft->ports[index] = req.node_port;
    
    ft->size++;
    return;
}

fingertable* init_ft() {
    fingertable *ft = malloc(sizeof(fingertable));
    if(ft == NULL) {
        return NULL;
    }
    
    for(int i = 0; i < FT_SIZE; i++) {
        ft->starts[i] = 0;
        ft->ids[i] = 0;
        ft->ips[i] = 0;
        ft->ports[i] = 0;
    }
    
    ft->size = 0;
    return ft;
}

int get_first_empty_entry(fingertable *ft) {
    if(ft == NULL) return -2;
    int index = 0;
    for(; index < FT_SIZE; index++) {
        if(ft->ports[index] == 0) return index;
    }
    return -1; // No empty entry 
}
