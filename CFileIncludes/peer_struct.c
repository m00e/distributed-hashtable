#include <stdio.h>
#include <stdlib.h>

//TODO: Initialize values??? and change values?
int update_peer(peer *self, uint16_t new_node_id, uint32_t new_node_ip, uint16_t new_node_port, int flag) {
    if(self == NULL) {
        return -1;
    }
    
    if(flag == 1) {
        self->pred->node_id = new_node_id;
        self->pred->node_ip = new_node_ip;
        self->pred->node_port = new_node_port;
    } else if(flag == 2) {
        self->succ->node_id = new_node_id;
        self->succ->node_ip = new_node_ip;
        self->succ->node_port = new_node_port;
    }
    
    return 0;
}

/*
 Updates the successor of a given peer.
 */
int update_peer_succ(peer *self, uint16_t new_node_id, uint32_t new_node_ip, uint16_t new_node_port) {
    return update_peer(self, new_node_id, new_node_ip, new_node_port, 2);
}

/*
 Updates the predecessor of a given peer.
 */
int update_peer_pred(peer *self, uint16_t new_node_id, uint32_t new_node_ip, uint16_t new_node_port) {
    return update_peer(self, new_node_id, new_node_ip, new_node_port, 1);
}

/*
 Initializes and returns a new peer.
 */
peer *create_peer(int argc, const char **argv) {
    peer *new_peer = malloc(sizeof(peer));
    if(new_peer == NULL) return NULL;
    peer *pred = malloc(sizeof(peer));
    peer *succ = malloc(sizeof(peer));
    if(pred == NULL || succ == NULL) return NULL;
    if(argc > 3 && (atoi(argv[3]) > MAX_ID)) {
        free_peer(new_peer);
        return NULL;
    }
    new_peer->pred = pred;
    new_peer->succ = succ;
    
    // To check if predeccesor and successor exist
    new_peer->pred->node_port = 0;
    new_peer->succ->node_port = 0;
    
    new_peer->node_ip = string_to_ip4(argv[1]);
    new_peer->node_port = atoi(argv[2]);
    if(argc == 3) new_peer->node_id = 0;
    else new_peer->node_id = atoi(argv[3]);
    
    new_peer->ft = NULL;
    return new_peer;
}

void print_peer_info(peer *self) {
    if(PRINT_PEER_INFO) {
        fprintf(stderr, "=======================\n");
        fprintf(stderr, "Information of peer: %d\n", self->node_id);
        if(self->pred->node_port == 0) fprintf(stderr, "Current predeccesor: NULL\n");
        else fprintf(stderr, "Current predeccesor: %d\n", self->pred->node_id);
        
        if(self->succ->node_port == 0) fprintf(stderr, "Current successor: NULL\n");
        else fprintf(stderr, "Current successor: %d\n", self->succ->node_id);
        fprintf(stderr, "=======================\n");
    }
    return;
}

void free_peer(peer *self) {
    free(self->pred);
    free(self->succ);
    free(self);
}
