#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>     // for INT_MAX
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>

#include "Headers/err.h"
#include "Headers/net.h"
#include "Headers/protocol.h"
#include "Headers/table.h"
#include "Headers/peer_struct.h"
#include "Headers/ctrl_handlers.h"
#include "Headers/fingertable_op.h"

#define USAGE_STRING "<IP> <Port> [ID] [<Peer-IP> <Peer-Port>]"

int listen_socket = -1;
fd_set master;
int i = -1; // loop variable that is used when iterating over sockets from select

void* request = NULL;
void* response = NULL;

pair* table = NULL;
socket_pair* socket_table_by_key = NULL;
socket_pair* socket_table = NULL;

void cleanup();
void INTHandler();
void* execute_hash_req(void* req_mem, packet* req, size_t* resp_len);

void cleanup() {
    free(request);
    request = NULL;
    free(response);
    response = NULL;
    if (i != -1) {
        close(i);
        FD_CLR(i, &master);
    }
}

// handle termination when waiting for connection.
void INTHandler() {
    if (listen_socket != -1) close(listen_socket);
    cleanup();
    free_table(table);
    close_table_by_key(socket_table_by_key);
    close_table(socket_table);
    exit(EXIT_SUCCESS);
}

// Execute request on hash table.
void* execute_hash_req(void* req_mem, packet* req, size_t* resp_len) {
    packet resp = init_packet_empty();
    pair* req_pair = NULL;
    switch(req->type) {
        case GET_REQ:
            // get value by key in hash table
            HASH_FIND(hh, table, req->key, req->key_len, req_pair);
            if (!req_pair) {
                // No pair with key exists
                ERROR("Key does not exist. Responding with empty body.\n");
                resp.type = GET_ACK;
                free(req->key);
            } else {
                // Take key from request and value from hash table find
                resp = init_hash_packet(GET_ACK, req->key_len, req->key, req_pair->value_len, req_pair->value);
            }
            break;
        case SET_REQ:
            // look first if already exists
            HASH_FIND(hh, table, req->key, req->key_len, req_pair);
            if (req_pair) {
                // Delete if existing
                HASH_DEL(table, req_pair);
                free_pair(req_pair);
            }

            req_pair = init_pair(*req);
            if (!req_pair) {
                ERROR("calloc error (in init_pair): %s\n", strerror(errno));
                free(req->key);
                free(req->value);
                return NULL;
            }
            HASH_ADD_KEYPTR(hh, table, req_pair->key, req_pair->key_len, req_pair);

            resp.type = SET_ACK;
            break;
        case DEL_REQ:
            // Check if <key, value> pair exists already
            HASH_FIND(hh, table, req->key, req->key_len, req_pair);
            if (req_pair) {
                // Delete if existing
                HASH_DEL(table, req_pair);
                free_pair(req_pair);
            } else {
                // Client will not see the difference
                DEBUG("Key does not exist. Still sending acknowlegment.\n");
            }
            resp.type = DEL_ACK;
            free(req->key);
            break;
        default:
            ERROR("Unknown request.\n");
            return NULL;
    }
    void* resp_pkg = build_packet(resp, resp_len);
    free(resp.key);
    return resp_pkg;
}

int main(int argc, const char* argv[]) {
    if (argc < 3 || argc == 5 || argc > 6) {
        // Usage ./peer <IP> <Port> [ID] [<Peer-IP> <Peer-Port>]
        ERROR_EXIT("Usage: %s %s\n", argv[0], USAGE_STRING);
    }
    
    peer *self = create_peer(argc, &(*argv));
    if(self == NULL) ERROR_EXIT("Malloc failed for peer\n");
    
    FD_ZERO(&master);
    int fdmax;

    listen_socket = build_connection_as_server(port_to_string(self->node_port));
    if (listen_socket == -1) {
        // error message already printed
        // cleanup not necessary
        free_peer(self);
        exit(EXIT_FAILURE);
    }
    FD_SET(listen_socket, &master);
    fdmax = listen_socket;

    signal(SIGINT, INTHandler);
    
    if (argc == 6) {
        int socketfd = build_connection_as_client(argv[4], argv[5]);
        if (-1 == socketfd) {
            free_peer(self);
            ERROR_EXIT("Failed to create socket for sending join packet.\n");
        }
        
        void* join_packet = create_join(argv[3], argv[1], argv[2]);
        size_t len = 11;

        sendn(socketfd, join_packet, len, 0);
        
        close(socketfd);
        free(join_packet);
    }
    
    /* Start sending stabilize messages if successor exists. */
    pthread_t pid;
    if(pthread_create(&pid, NULL, &stabilize, self) != 0) {
        free_peer(self);
        ERROR_EXIT("Could not create peer");
    }
    
    int ft_requester = 0; // Client socket that requested to build fingertable
    
    while (1) {
        fd_set read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            ERROR("select error: %s\n", strerror(errno));
            INTHandler();
        }
        
        // Since new sockets are added into master, they will not be set in read_set (in this while iteration)
        // and thus we also do not need to iterate over them
        int fdmax_copy = fdmax;
        // loop over the existing connections looking for data to read
        for (i = 0; i <= fdmax_copy; i++) {
            if (!FD_ISSET(i, &read_fds)) {
                continue;
            }
            if (i == listen_socket) {
                // new connection
                int accepted_socket = accept_connection(listen_socket);
                if (accepted_socket == -1) {
                    // error message already printed
                    // cleanup not necessary
                    continue;
                }
                DEBUG("Connection accepted.\n");
                // add new socket to master set
                FD_SET(accepted_socket, &master);
                if (accepted_socket > fdmax) {
                    // update max fd
                    fdmax = accepted_socket;
                }
                continue;
            }
            // hande data from a client
            packet req;
            size_t req_size;
            if (!(request = recv_packet(i, &req, &req_size))) {
                if (errno == ENOMEM) {
                    // we are out of memory
                    ERROR("malloc error: %s\n", strerror(errno));
                    INTHandler();
                } else {
                    // error message printed already
                    // delete socket from internal hash table
                    socket_pair* s;
                    HASH_FIND(hh_sock, socket_table, &i, sizeof(int), s);
                    if (s) {
                        HASH_DELETE(hh_sock, socket_table, s);
                        close_pair(s);
                    }

                    cleanup();
                    continue;
                }
            }
            
            // Also extract IP and Port of received ctrl-message.
            if(is_known_control_type(req.type)) {
                NODE_IP_TYPE* node_ip_pointer = request + NODE_IP_OFFSET;
                req.node_ip = NTOH(NODE_IP)(*node_ip_pointer);
                
                NODE_PORT_TYPE* node_port_pointer = request + NODE_PORT_OFFSET;
                req.node_port = NTOH(NODE_PORT)(*node_port_pointer);
                
            }
            
            HASH_ID_TYPE hash_key;
            int range;
            int do_memcpy = 0;
            
            if(req.type != STAB && req.type != JOINING && req.type != NOTIFY && req.type != FINGER && req.type != F_ACK && req.type != F_LKP && req.type != F_LKP_RPLY) {
                // get hash_key from memory (depending on the type it lays at different locations)
                hash_key = get_ring_key(req.key_len, request + (is_known_hash_type(req.type) ? HEADER_LEN : HASH_ID_OFFSET));
                // Only copy into the packet struct, if we are responsible for this key.
                // If not it is suffiecient to only set pointers since we will just forward the request
                if(self->succ->node_port == 0 && self->pred->node_port == 0) {
                    range = 1; // I am responsible, no matter what.
                } else {
                    range = what_range(hash_key, self->node_id, self->pred->node_id, self->succ->node_id);
                }
                
                if (is_known_req_type(req.type) && range == 1) {
                    do_memcpy = 1;
                }
                // initialize body struct from request body memory
                if (!extract_body(request, &req, do_memcpy)) {
                    // error message printed already
                    // delete socket from internal hash table
                    socket_pair* s;
                    HASH_FIND(hh_sock, socket_table, &i, sizeof(int), s);
                    if (s) {
                        HASH_DELETE(hh_sock, socket_table, s);
                        close_pair(s);
                    }

                    cleanup();
                    continue;
                }
            }
            
            if (is_known_control_req(req.type)) {
                switch(req.type) {
                    case LKP: // LOOKUP
                        handle_lookup(request, req, range, self);
                        cleanup();
                        continue;
                    case STAB: // STABILIZE
                        handle_stabilize(self, req);
                        cleanup();
                        continue;
                    case NOTIFY:// NOTIFY
                        handle_notify(self, req);
                        cleanup();
                        continue;
                    case JOINING: // JOIN
                        handle_join(self, req, request);
                        cleanup();
                        continue;
                    case FINGER: // FINGER
                        // If fingertable already exists, it just reallocates
                        self->ft = init_ft();
                        if(self->ft == NULL) {
                            ERROR("Failed to allocate fingertable memory\n");
                            continue;
                        }
                        
                        ft_requester = i; // Save info about client that wanted self to build fingertable
                        
                        for(int j = 0; j < FT_SIZE; j++) {
                            if(self->succ->node_port == 0 && self->pred->node_port == 0) {
                                //Special case: Self is only peer in chord ring; Maybe not even needed
                                self->ft->starts[j] = get_ith_ftentry(j, self->node_id, FT_SIZE);
                                add_ft_entry__peer(self->ft, self, j);
                            } else {
                                //Calculate start values for lookups
                                self->ft->starts[j] = get_ith_ftentry(j, self->node_id, FT_SIZE);
                                
                                range = what_range(self->ft->starts[j], self->node_id, self->pred->node_id, self->succ->node_id);
                                switch(range) {
                                    case 1: // Self is responsible for the given start value
                                        add_ft_entry__peer(self->ft, self, j);
                                        break;
                                    case 2: // Successor is responsible for the given start value
                                        add_ft_entry__peer(self->ft, self->succ, j);
                                        break;
                                } // End; switch(range)
                            }
                            
                            
                        } // End; for(int i = 0; i < FT_SIZE; i++)
                        
                        // Protect client from being closed
                        int i_bkp = i;
                        i = -1;
                        cleanup();
                        i = i_bkp;
                        
                        if(self->ft->size == FT_SIZE) { // Check if fingertable is finally build
                            //Send Finger-ACK back to client and close connection; In case there is no need to send lookups.
                            fprintf(stderr, "Successfully built up fingertable.\n");
                            
                            void *finger_resp = create_f_ack();
                            size_t len = 11;
                            sendn(i, finger_resp, len, 0);
                            free(finger_resp);
                            
                            close(i);
                        } else {
                            // Send first lookup if fingertable is not complete yet
                            int index = get_first_empty_entry(self->ft);
                            void* flkp_pack = create_fingerlkp(self->ft->starts[index], self->node_id, self->node_ip, self->node_port);
                            
                            char *succ_ip = ip4_to_string(self->succ->node_ip);
                            char *succ_port = port_to_string(self->succ->node_port);
                                                        
                            int socketfd = build_connection_as_client(succ_ip, succ_port);
                            free(succ_ip);
                            free(succ_port);
                            if (-1 == socketfd) {
                                continue;
                            }
                            size_t len = 11;
                            sendn(socketfd, flkp_pack, len, 0);
                                                        
                            free(flkp_pack);
                            close(socketfd);
                        }
                        
                        FD_CLR(ft_requester, &master);
                        continue;
                }
                
            } else if (is_known_control_rsp(req.type)) {
                switch(req.type) {
                    case LKP_RPLY:
                    { // LOOKUP - Reply
                        if(self->ft != NULL && self->ft->size < FT_SIZE) { // Lookup - Reply of Finger-Lookup //Code of T18G08
                            int index = get_index_from_value(self->ft->starts, req.hash_id);
                            add_ft_entry__packet(self->ft, req, index);
                            if(self->ft->size == FT_SIZE) { // Check if fingertable is finally build
                                //Send Finger-ACK back to client and close connection; In case there is no need to send lookups.
                                fprintf(stderr, "Successfully built up fingertable.\n");
                                void *finger_resp = create_f_ack();
                                size_t len = 11;
                                sendn(ft_requester, finger_resp, len, 0);
                                free(finger_resp);
                                
                                close(ft_requester);
                                ft_requester = 0;
                            } else { //Send next lookup
                                int index_first_empty = get_first_empty_entry(self->ft);
                                void* flkp_pack = create_fingerlkp(self->ft->starts[index_first_empty], self->node_id, self->node_ip, self->node_port);
                                
                                char *succ_ip = ip4_to_string(self->succ->node_ip);
                                char *succ_port = port_to_string(self->succ->node_port);
                                                            
                                int socketfd = build_connection_as_client(succ_ip, succ_port);
                                free(succ_ip);
                                free(succ_port);
                                if (-1 == socketfd) {
                                    continue;
                                }
                                size_t len = 11;
                                sendn(socketfd, flkp_pack, len, 0);
                                                            
                                free(flkp_pack);
                                close(socketfd);
                            }
                        } else { // Lookup - Reply of DHT-Lookup
                            // search in table and find correct request
                            socket_pair* accepted_socket_pair;
                            HASH_FIND(hh_key, socket_table_by_key, &hash_key, sizeof hash_key, accepted_socket_pair);
                            if (!accepted_socket_pair) {
                                ERROR("Something went wrong. We did not send a lookup with this hash-id.\n");
                                cleanup();
                                continue;
                            }
                            if (!accepted_socket_pair->packet || !accepted_socket_pair->packet_len) {
                                ERROR("Something went wrong. The socket_pair does not contain a packet that we can forward.\n");
                                close_pair(accepted_socket_pair);
                                cleanup();
                                continue;
                            }
                            // forward request to peer in message
                            char* key_holder = ip4_to_string(req.node_ip);
                            char* key_holder_port = port_to_string(req.node_port);
                            int socketfd = forward_packet(key_holder, key_holder_port, accepted_socket_pair->packet, accepted_socket_pair->packet_len);
                            free(key_holder);
                            free(key_holder_port);
                            if (socketfd == -1) {
                                // error message printed already
                                cleanup();
                                continue;
                            }
                            // Since we are waiting for a response we need to add the socket to master
                            FD_SET(socketfd, &master);
                            if (socketfd > fdmax) {
                                // update max fd
                                fdmax = socketfd;
                            }
                            // And since we also need the connection between the hash_key/client-request we need to also add it     to our table
                            socket_pair* key_holder_pair = init_socket_pair(socketfd, hash_key, 4, NULL, 0);
                            HASH_ADD(hh_sock, socket_table, socketfd, sizeof(int), key_holder_pair);

                            free(accepted_socket_pair->packet);
                            accepted_socket_pair->packet = NULL;
                            accepted_socket_pair->packet_len = 0;
                        }
                        cleanup();
                        continue;
                    }
                }
                
            } else if (is_known_req_type(req.type)) {
                // GET, SET, DELETE
                char* key_holder;
                char* key_holder_port;
                switch(range) {
                    case 1: { // We hold this key
                        DEBUG("Key lays in our table. Processing request directly.\n");
                        size_t resp_len;
                        if (!(response = execute_hash_req(request, &req, &resp_len))) {
                            // error message already printed
                            cleanup();
                            continue;
                        }
                        // respond
                        if (sendn(i, response, resp_len, 0)) {
                            DEBUG("Response sent.\n");
                        }
                        // We do not care if send is succesful or not
                        cleanup();
                        continue;
                    } case 2: { // Next peer holds key
                        DEBUG("Next peer holds key. Forwarding request.\n");
                        // forward request to next node
                        key_holder = (char*)ip4_to_string(self->succ->node_ip);
                        key_holder_port = (char*)port_to_string(self->succ->node_port);

                        DEBUG("Building new connection as client.\n");
                        // forward request
                        int socketfd;
                        if (-1 == (socketfd = forward_packet(key_holder, key_holder_port, request, req_size))) {
                            // error message printed already
                            cleanup();
                            continue;
                        }
                        // Since we are waiting for a respond we need to add both sockets to our internal hash table
                        socket_pair* accepted_socket_pair = init_socket_pair(i, hash_key, 2, NULL, 0);
                        HASH_ADD(hh_sock, socket_table, socketfd, sizeof(int), accepted_socket_pair);
                        HASH_ADD(hh_key, socket_table_by_key, hash_key, sizeof hash_key, accepted_socket_pair);
                        FD_SET(socketfd, &master);
                        if (socketfd > fdmax) {
                            // update max fd
                            fdmax = socketfd;
                        }
                        socket_pair* key_holder_pair = init_socket_pair(socketfd, hash_key, 4, NULL, 0);
                        HASH_ADD(hh_sock, socket_table, socketfd, sizeof(int), key_holder_pair);
                        // Protect socket from being closed
                        int i_bkp = i;
                        i = -1;
                        cleanup();
                        i = i_bkp;
                        continue;
                    } default: { // we do not know who holds this key
                        DEBUG("Unknown key. Sending lookup to next peer.\n");
                        // send lookup to next node
                        void* lookup_pkg;
                        if (!(lookup_pkg = create_lookup(get_ring_key(req.key_len, req.key), id_to_string(self->node_id), ip4_to_string(self->node_ip), port_to_string(self->node_port)))) {
                            // error message printed already
                            cleanup();
                            continue;
                        }
                        
                        if(self->ft == NULL || self->ft->size < FT_SIZE) {
                            if (!ask_next(lookup_pkg, ip4_to_string(self->succ->node_ip), port_to_string(self->succ->node_port))) {
                                // error message printed already
                                free(lookup_pkg);
                                cleanup();
                                continue;
                            }
                        } else { // Use fingertable because it is already build.
                            int index = 0;
                            for(; index < FT_SIZE; index++) {
                                if(self->ft->starts[index] > req.hash_id) {
                                    index--;
                                    break;
                                }
                            }
                            if(index == -1) index = 0; // Special case! (If 
                            
                            //Forward request to peer with closest id to hash_id (like in unit05, page 19)
                            if (!ask_next(lookup_pkg, ip4_to_string(self->ft->ips[index]), port_to_string(self->ft->ports[index]))) {
                                // error message printed already
                                free(lookup_pkg);
                                cleanup();
                                continue;
                            }
                        } // End; if(self->ft == NULL || self->ft->size < FT_SIZE)
                        
                        DEBUG("Lookup sent.\n");
                        free(lookup_pkg);

                        // Since we are waiting for a respond we need to add the socket to our internal hash table
                        socket_pair* accepted_socket_pair = init_socket_pair(i, hash_key, 2, request, req_size);
                        HASH_ADD(hh_sock, socket_table, socketfd, sizeof(int), accepted_socket_pair);
                        HASH_ADD(hh_key, socket_table_by_key, hash_key, sizeof hash_key, accepted_socket_pair);
                        // Protect socket from being closed
                        request = NULL;
                        int i_bkp = i;
                        i = -1;
                        cleanup();
                        i = i_bkp;
                        break;
                    }
                }
            } else if (is_known_ack_type(req.type)) {
                // GET-, SET-, DELETE- acknowlegments

                // this has to be the respond to the forwarded request
                socket_pair* key_holder_pair;
                // search in hash table
                HASH_FIND(hh_sock, socket_table, &i, sizeof(int), key_holder_pair);
                if (!key_holder_pair) {
                    ERROR("Something went wrong. We did not send a request to this peer.\n");
                    cleanup();
                    continue;
                }
                // find client_socket
                socket_pair* client_pair;
                socket_pair* tmp = socket_table_by_key;
                do {
                    HASH_FIND(hh_key, tmp, &key_holder_pair->hash_key, sizeof key_holder_pair->hash_key, client_pair);
                    if (!client_pair) {
                        ERROR("Something went wrong. We can't find the socket to the client.\n");
                        cleanup();
                        continue;
                    }
                    // Since the hash_key is saved in two entrys I could happen that we not find the client socket first.
                    // In this case we need to continue searching behind forward_pair
                    // (I'm pretty sure that this case will never happen in practice because we add the client socket first to the table, but better be safe that sorry)
                    tmp = client_pair->hh_key.next;
                } while (client_pair == key_holder_pair);

                // forward reply
                DEBUG("Forwarding response to client.\n");
                sendn(client_pair->socketfd, request, req_size, 0);
                // we don't care about errors in sendn. We are doing the same thing in both cases

                // close, remove from master and from hash_table the client and the key_holder socket
                close(client_pair->socketfd);
                FD_CLR(client_pair->socketfd, &master);
                HASH_DELETE(hh_sock, socket_table, key_holder_pair);
                close_pair(key_holder_pair);
                HASH_DELETE(hh_key, socket_table_by_key, client_pair);
                HASH_DELETE(hh_sock, socket_table, client_pair);
                close_pair(client_pair);

                cleanup();
                continue;
            } else {
                ERROR("Unknown type.\n");
                cleanup();
                continue;
            }
        } // end for loop [0, fdmax]
    } // end of main accept loop (while)
} // end of main
