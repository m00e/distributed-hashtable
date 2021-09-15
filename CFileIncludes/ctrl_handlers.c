/* Code of T18G08 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../Headers/ctrl_handlers.h"
#include "../Headers/peer_struct.h"
#include "../Headers/protocol.h"
#include "../Headers/err.h"
#include "../Headers/net.h"
#include "../Headers/fingertable_op.h"

void *stabilize(void *sending_peer) { /*  STABILIZE-THREAD */
    peer *s_peer = (peer*) sending_peer;
    while(1) {
        int sleeping = sleep(2); //Sleeping for 2 secs.
        if(sleeping != 0) {
            ERROR("A problem occured while sleeping.\n");
        }
        
        // Check if given peer has a successor.
        if(s_peer->succ->node_port == 0) {
            continue;
        }
        // Create stabilize package & send stabilize to successor
        packet pck_stabilize = init_packet_empty();
        pck_stabilize.type = STAB;
        pck_stabilize.hash_id = 0;
        pck_stabilize.node_id = s_peer->node_id;
        pck_stabilize.node_ip = s_peer->node_ip;
        pck_stabilize.node_port = s_peer->node_port;
        
        char* receiver_ip = ip4_to_string(s_peer->succ->node_ip);
        char* receiver_port = port_to_string(s_peer->succ->node_port);
        
        int socketfd = build_connection_as_client(receiver_ip, receiver_port);
        free(receiver_ip);
        free(receiver_port);
        if (-1 == socketfd) {
            continue;
        }

        void* stabilize_packet;
        size_t len;
        if (!(stabilize_packet = build_packet(pck_stabilize, &len))) {
            close(socketfd);
            continue;
        }

        sendn(socketfd, stabilize_packet, len, 0);
        
        close(socketfd);
        free(stabilize_packet);
    }
}

void handle_lookup(void* req_mem, packet req, int range, peer *self) {
    packet resp = init_packet_empty();
    if(range != 0) {
        DEBUG("Next peer is holding key. Responding to sender of lookup.\n");
        
        resp.type = LKP_RPLY;
        resp.hash_id = req.hash_id;
        resp.node_id = (range == 1 ? self->node_id : self->succ->node_id);
        resp.node_ip = (range == 1 ? self->node_ip : self->succ->node_ip);
        resp.node_port = (range == 1 ? self->node_port : self->succ->node_port);
        
        
        char* sender_ip = ip4_to_string(req.node_ip);
        char* sender_port = port_to_string(req.node_port);
        int socketfd = build_connection_as_client(sender_ip, sender_port);
        free(sender_ip);
        free(sender_port);
        if (-1 == socketfd) {
            // error message printed already
            return;
        }

        void* lkp_response;
        size_t len;
        if (!(lkp_response = build_packet(resp, &len))) {
            // error message printed already
            close(socketfd);
            return;
        }
        
        sendn(socketfd, lkp_response, len, 0);
        // error handling would do the same as we do in next few lines anyway
        //      and thus error handling is not necessary
        close(socketfd);
        free(lkp_response);
    } else { // Forward message
        DEBUG("Peer that holds this key is not known. Asking next peer.\n");
        
        if(self->ft == NULL || self->ft->size < FT_SIZE) {
            ask_next(req_mem, ip4_to_string(self->succ->node_ip), port_to_string(self->succ->node_port));
        } else { // Use fingertable because it is already build.
            int i = 0;
            for(; i < FT_SIZE; i++) {
                if(self->ft->starts[i] > req.hash_id) {
                    i--;
                    break;
                }
            }
            if(i == -1) i = 0;
            //Forward request to peer with closest id to hash_id (like in unit05, page 19)
            ask_next(req_mem, ip4_to_string(self->ft->ips[i]), port_to_string(self->ft->ports[i]));
        }
    }
    return;
}

int handle_stabilize(peer *self, packet req) {
    //Just for the joining peer: update predecessor upon receiving stabilize from future predecessor.
    if(self->pred->node_port == 0 || is_own_range__stabilize(self, req.node_id) == 0) {
        if(update_peer_pred(self, req.node_id, req.node_ip, req.node_port) != 0) {
            ERROR("Joining peer failed to update its predecessor.");
            return -1;
        }
        //TODO: DO NOT SEND A NOTIFY BACK ????
        return 0;
    }
                        
    char *sender_ip = ip4_to_string(req.node_ip);
    char *sender_port = port_to_string(req.node_port);
                
    int socketfd = build_connection_as_client(sender_ip, sender_port);
    if (-1 == socketfd) {
        free_peer(self);
        ERROR("Failed to create socket for sending stabilize packet.\n");
        return -1;
    }
                
    free(sender_ip);
    free(sender_port);
                        
    void* notify_response = create_notify(self->pred->node_id, self->pred->node_ip, self->pred->node_port);
    size_t len = 11;

    sendn(socketfd, notify_response, len, 0);
                        
    close(socketfd);
    free(notify_response);
                        
    print_peer_info(self);
    return 0;
}

int handle_notify(peer *self, packet req) {
    // New peer receives notify(); Update successor instantly
    if(self->succ->node_port == 0) {
        if(update_peer_succ(self, req.node_id, req.node_ip, req.node_port) != 0) {
            ERROR("Peer update failed.\n");
            print_peer_info(self);
            return -1;
        }
    }
                        
    // Check if received node-ID is between self and succ, if yes, then update successor
    if(is_own_range__notify(self, req.node_id) == 0) {
        if(update_peer_succ(self, req.node_id, req.node_ip, req.node_port) != 0) {
            ERROR("Peer update failed.\n");
            return -1;
        }
    }
                        
    print_peer_info(self);
    return 0;
}

int handle_join(peer *self, packet req, void* request) {
    // Case there is no succ and no pred
    if(self->succ->node_port == 0 && self->pred->node_port == 0) {
        update_peer_pred(self, req.node_id, req.node_ip, req.node_port);
        update_peer_succ(self, req.node_id, req.node_ip, req.node_port);
                
        char* sender_ip = ip4_to_string(req.node_ip);
        char* sender_port = port_to_string(req.node_port);
                            
        //Return notify message to joining peer
        int socketfd = build_connection_as_client(sender_ip, sender_port);
        free(sender_ip);
        free(sender_port);
        if (-1 == socketfd) {
            return -1;
        }
                            
        void* notify_response = create_notify(self->node_id, self->node_ip, self->node_port);
        size_t len = 11;
        sendn(socketfd, notify_response, len, 0);
                            
        print_peer_info(self);
        close(socketfd);
        free(notify_response);
        return 0;
    }
    
    // Check if self is responsible for received node_id, if yes, then update predecessor, if no, send msg to successor.
    if(is_own_range__join(self, req.node_id) == 0) {
        update_peer_pred(self, req.node_id, req.node_ip, req.node_port);
        char* sender_ip = ip4_to_string(req.node_ip);
        char* sender_port = port_to_string(req.node_port);
        //Return notify message to joining peer
                            
        int socketfd = build_connection_as_client(sender_ip, sender_port);
        free(sender_ip);
        free(sender_port);
        if (-1 == socketfd) {
            return -1;
        }

        void* notify_response = create_notify(self->node_id, self->node_ip, self->node_port);
        size_t len = 11;

        sendn(socketfd, notify_response, len, 0);
                            
        close(socketfd);
        free(notify_response);
    } else {
        // Send received package to successor.
        char* sender_ip = ip4_to_string(self->succ->node_ip);
        char* sender_port = port_to_string(self->succ->node_port);
                            
        int socketfd = build_connection_as_client(sender_ip, sender_port);
        free(sender_ip);
        free(sender_port);
        if (-1 == socketfd) {
            return -1;
        }

        size_t len = 11;
        sendn(socketfd, request, len, 0);
                            
        close(socketfd);
    }
                        
    print_peer_info(self);
    return 0;
}
