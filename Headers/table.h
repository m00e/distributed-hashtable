#ifndef BLOCK3_TABLE_H
#define BLOCK3_TABLE_H

#include "uthash.h"
#include "protocol.h"

#define MY_ID     1
#define MY_IP     2
#define MY_PORT   3
#define PREV_ID   4
#define PREV_IP   5
#define PREV_PORT 6
#define NEXT_ID   7
#define NEXT_IP   8
#define NEXT_PORT 9

#define OWN_IP      1
#define OWN_PORT    2
#define OWN_ID      3
#define KNOWN_IP    4
#define KNOWN_PORT  5

typedef struct _pair {
    packet;
    UT_hash_handle hh;
} pair;

typedef struct _socket_pair {
    int socketfd;
    HASH_ID_TYPE hash_key;
    int type;
    /* type =   1 - listen for inbound requests
    			2 - connection with peer/client that sent request
				3 - listen for lookup replies
				4 - connection with peer that we forward the request to
	*/
	void* packet;
	size_t packet_len;
    UT_hash_handle hh_sock;
    UT_hash_handle hh_key;
} socket_pair;

int what_range(HASH_ID_TYPE key, HASH_ID_TYPE my_id, HASH_ID_TYPE prev_id, HASH_ID_TYPE next_id);
uint16_t get_ring_key(KEY_LEN_TYPE key_len, void* key);
pair* init_pair(packet pkg);
socket_pair* init_socket_pair(int socketfd, HASH_ID_TYPE hash_key, int type, void* packet, size_t packet_len);
packet init_body_struct_from_pair(pair* key_value_pair);
void free_pair(pair* p);
void free_table(pair* table);
void close_pair(socket_pair* p);
void close_table(socket_pair* socket_table);
void close_table_by_key(socket_pair* socket_table);

#endif //BLOCK3_TABLE_H
