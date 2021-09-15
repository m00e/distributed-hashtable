#ifndef FINGERTABLE_OP_
#define FINGERTABLE_OP_

#include <math.h>

#include "peer_struct.h"
#include "protocol.h"

typedef u_int16_t uint16_t;
int power2(int n); 
int get_ith_ftentry(int i, int k, int m);
int get_index_from_value(uint16_t start_vals[FT_SIZE], uint16_t start_val);
void add_ft_entry__peer(fingertable *ft, peer *peer_to_add, int index);
void add_ft_entry__packet(fingertable *ft, packet req, int index);
fingertable* init_ft();
int get_first_empty_entry(fingertable *ft);

#endif
