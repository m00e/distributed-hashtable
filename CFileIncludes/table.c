#include <stdio.h>      // for fprintf
#include <netinet/in.h> // for ntohs, ntohl, htons, htonl
#include <unistd.h>     // for close
#include <errno.h>
#include "../Headers/err.h"
#include "../Headers/table.h"
#include "../Headers/protocol.h"

int what_range(HASH_ID_TYPE key, HASH_ID_TYPE my_id, HASH_ID_TYPE prev_id, HASH_ID_TYPE next_id) {
    /*
        0 - no idea
        1 - my range
        2 - next range

    */
    DEBUG("Who holds key %d?\n", key);

    if (key <= my_id && key > prev_id) {
        return 1;
    } else if (my_id < prev_id && (key <= my_id || key > prev_id)) {
        // first in the ring
        return 1;
    } else if (key > my_id && key <= next_id) {
        return 2;
    } else if (my_id > next_id && (key > my_id || key <= next_id)) {
        // last in the ring
        return 2;
    }

    return 0;
}

HASH_ID_TYPE get_ring_key(KEY_LEN_TYPE key_len, void* key) {
    if (key_len && !key) {
        ERROR("key points to NULL. Returning 0.\n");
        return 0;
    }
    switch (key_len) {
        case 0:
            return 0;
        case 1:
            return *(uint8_t*)key;
        default: {
            HASH_ID_TYPE* ring_key_pointer = key;
            return NTOH(HASH_ID)(*ring_key_pointer);
        }
    }
}

pair* init_pair(packet pkg) {
    pair* new_pair = calloc(1, sizeof(pair));
    if (!new_pair) return NULL;

    new_pair->key_len = pkg.key_len;
    new_pair->key = pkg.key;
    new_pair->value_len = pkg.value_len;
    new_pair->value = pkg.value;

    return new_pair;
}

socket_pair* init_socket_pair(int socketfd, HASH_ID_TYPE hash_key, int type, void* packet, size_t packet_len) {
    socket_pair* new_pair = calloc(1, sizeof(socket_pair));
    if (!new_pair) {
        ERROR("calloc error: %s\n", strerror(errno));
        return NULL;
    }

    new_pair->socketfd = socketfd;
    new_pair->hash_key = hash_key;
    new_pair->type = type;
    new_pair->packet = packet;
    new_pair->packet_len = packet_len;

    return new_pair;
}

packet init_body_struct_from_pair(pair* key_value_pair) {
    packet new_pkg;

    new_pkg.key_len = key_value_pair->key_len;
    new_pkg.key = key_value_pair->key;
    new_pkg.value_len = key_value_pair->value_len;
    new_pkg.value = key_value_pair->value;

    return new_pkg;
}

void free_pair(pair* p) {
    if (p) {
        free(p->key);
        free(p->value);
        free(p);
    }
}

void free_table(pair* hash_table) {
    pair* current;
    pair* tmp;
    HASH_ITER(hh, hash_table, current, tmp) {
        HASH_DEL(hash_table, current);
        free_pair(current);
    }
}

void close_pair(socket_pair* p) {
    if (p) {
        if (-1 == p->socketfd) close(p->socketfd);
        free(p->packet);
        free(p);
    }
}

void close_table(socket_pair* socket_table) {
    socket_pair* curr;
    socket_pair* tmp;
    HASH_ITER(hh_sock, socket_table, curr, tmp) {
        HASH_DELETE(hh_sock, socket_table, curr);
        close_pair(curr);
    }

}

void close_table_by_key(socket_pair* socket_table) {
    socket_pair* curr;
    socket_pair* tmp;
    HASH_ITER(hh_key, socket_table, curr, tmp) {
        HASH_DELETE(hh_key, socket_table, curr);
    }
}
