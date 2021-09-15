#include <errno.h>      // for errno
#include <netinet/in.h> // for ntohs, ntohl, htons, htonl
#include <stdlib.h>     // for calloc
#include <string.h>     // for strlen, strncmp, strerror, memcpy
#include "../Headers/protocol.h"
#include "../Headers/err.h"        // for _ERROR
#include "../Headers/peer_struct.h"

int is_known_type(PKG_METHOD_TYPE pkg_type) {
    return is_known_control_type(pkg_type) || is_known_hash_type(pkg_type);
}

int is_known_control_type(PKG_METHOD_TYPE pkg_type) {
    return is_known_control_req(pkg_type) || is_known_control_rsp(pkg_type);
}

int is_known_control_req(PKG_METHOD_TYPE pkg_type) {
    switch (pkg_type) {
        case LKP:       return 1;
        case STAB:      return 1;
        case NOTIFY:    return 1;
        case JOINING:   return 1;
        case FINGER:    return 1;
        case F_ACK:     return 1;
        case F_LKP:     return 1;
        default:        return 0;
    }
}

int is_known_control_rsp(PKG_METHOD_TYPE pkg_type) {
    switch (pkg_type) {
        case LKP_RPLY:   return 1;
        case F_LKP_RPLY: return 1;
        default:         return 0;
    }
}

int is_known_hash_type(PKG_METHOD_TYPE pkg_type) {
    return is_known_req_type(pkg_type) || is_known_ack_type(pkg_type);
}

int is_known_req_type(PKG_METHOD_TYPE pkg_type) {
    switch (pkg_type) {
        case GET_REQ:
        case SET_REQ:
        case DEL_REQ: return 1;
        default:      return 0;
    }
}

int is_known_ack_type(PKG_METHOD_TYPE pkg_type) {
    switch (pkg_type) {
        case GET_ACK:
        case SET_ACK:
        case DEL_ACK: return 1;
        default:      return 0;
    }
}

PKG_METHOD_TYPE method_name_to_bits(const char* type_name) {
    size_t n = strlen(type_name);
    if (n == 3 && !strncmp(type_name, "GET", n)) {
        return GET_REQ;
    } else if (n == 3 && !strncmp(type_name, "SET", n)) {
        return SET_REQ;
    } else if (n == 6 && !strncmp(type_name, "DELETE", n)) {
        return DEL_REQ;
    } else if (n == 6 && !strncmp(type_name, "LOOKUP", n)) {
        return LKP;
    } else if (n == 4 && !strncmp(type_name, "STABILIZE", n)) {
        return STAB;
    } else if (n == 6 && !strncmp(type_name, "NOTIFY", n)) {
        return NOTIFY;
    } else if (n == 4 && !strncmp(type_name, "JOIN", n)) {
        return JOINING;
    } else if (n == 6 && !strncmp(type_name, "FINGER", n)) {
        return FINGER;
    } else if (n == 13 && !strncmp(type_name, "FINGER_LOOKUP", n)) {
        return F_LKP;
    } else {
        return INVALID_HEADER;
    }
}

// Fills name with the method name of the packet but at most n bytes.
char* bits_to_method_name(PKG_METHOD_TYPE type, char* name, size_t n) {
    if (!name) {
        ERROR("Name points to NULL.\n");
        return NULL;
    } else if (!n) {
        return name;
    }

    switch (type) {
        case GET_REQ:
            strncpy(name, "GET", n); break;
        case SET_REQ:
            strncpy(name, "SET", n); break;
        case DEL_REQ:
            strncpy(name, "DELETE", n); break;
        case GET_ACK:
            strncpy(name, "GET-ACK", n); break;
        case SET_ACK:
            strncpy(name, "SET-ACK", n); break;
        case DEL_ACK:
            strncpy(name, "DELETE-ACK", n); break;
        case LKP:
            strncpy(name, "LOOKUP", n); break;
        case LKP_RPLY:
            strncpy(name, "LOOKUP-REPLY", n); break;
        case STAB:
            strncpy(name, "STABILIZE", n); break;
        case NOTIFY:
            strncpy(name, "NOTIFY", n); break;
        case JOINING:
            strncpy(name, "JOIN", n); break;
        case F_ACK:
            strncpy(name, "FINGER-ACK", n); break;
        case FINGER:
            strncpy(name, "FINGER", n); break;
        case F_LKP:
            strncpy(name, "FINGER_LOOKUP", n); break;
        case F_LKP_RPLY:
            strncpy(name, "FINGER_LOOKUP_REPLY", n); break;
        default:
            ERROR("Unknown type.\n");
            return NULL;
    }
    return name;
}

// Extract all information from raw memory of the header into struct
packet extract_header(void* pkg_header, int check_compliance) {
    packet pkg = init_packet_empty();

    if (!pkg_header) {
        ERROR("pkg_header points to NULL.\n");
        pkg.type = INVALID_HEADER;
        return pkg;
    }
    // Save type in struct
    pkg.type = *(PKG_METHOD_TYPE*)pkg_header;
    // differentiate between different types
    if (is_known_control_type(pkg.type)) {
        // Concert hash_id to host-byte-order and save in struct
        HASH_ID_TYPE* hash_id_pointer = pkg_header + HASH_ID_OFFSET;
        pkg.hash_id = NTOH(HASH_ID)(*hash_id_pointer);

        // Convert node_id to host-byte-order and save in struct
        NODE_ID_TYPE* node_id_pointer = pkg_header + NODE_ID_OFFSET;
        pkg.node_id = NTOH(NODE_ID)(*node_id_pointer);
    } else if (is_known_hash_type(pkg.type)) {
        // Convert key_len to host-byte-order and save in struct
        KEY_LEN_TYPE* key_len_pointer = pkg_header + KEY_LEN_OFFSET;
        pkg.key_len = NTOH(KEY)(*key_len_pointer);
        // pkg.key_len = *(KEY_LEN_TYPE*)pkg_header + KEY_LEN_OFFSET;
        
        // Convert value_len to host-byte-order and save in struct
        VALUE_LEN_TYPE* value_len_pointer = pkg_header + VALUE_LEN_OFFSET;
        pkg.value_len = NTOH(VALUE)(*value_len_pointer);
        
        // Check if a packet with this header would be compliant with the protocol specs
        if (check_compliance && !is_protocol_compliant(pkg.type, pkg.key_len, pkg.value_len)) {
            // error message printed already
            pkg.type = INVALID_HEADER;
        }
    } else {
        ERROR("Package contains unknown type.\n");
        pkg.type = INVALID_HEADER;
    }
    
    

    return pkg;
}

// Extract all remaining information from raw memory of the body/rest into struct
// Assumes that pkg_mem points to the start of the received packet
packet* extract_body(void* pkg_mem, packet* pkg, int do_memcpy) {
    if (!pkg_mem) {
        ERROR("pkg_mem points to zero.\n");
        return NULL;
    }

    if (pkg->type == INVALID_HEADER) {
        ERROR("invalid packet type.\n");
        return NULL;
    } else if (is_known_control_type(pkg->type)) {
        // Convert node_ip to host-byte-order and save in struct
        NODE_IP_TYPE* node_ip_pointer = pkg_mem + NODE_IP_OFFSET;
        pkg->node_ip = NTOH(NODE_IP)(*node_ip_pointer);

        // Convert node_port to host-byte-order and save in struct
        NODE_PORT_TYPE* node_port_pointer = pkg_mem + NODE_PORT_OFFSET;
        pkg->node_port = NTOH(NODE_PORT)(*node_port_pointer);

    } else if (is_known_hash_type(pkg->type)) {
        // Move pointer behind packet header
        pkg_mem += HEADER_LEN;

        // Copy key into packet
        if (pkg->key_len) {
            if (do_memcpy) {
                pkg->key = malloc(pkg->key_len);
                if (!pkg->key) {
                    ERROR("malloc error: %s\n", strerror(errno));
                    return NULL;
                }
                memcpy(pkg->key, pkg_mem, pkg->key_len);
            } else {
                pkg->key = pkg_mem;
            }
        } else {
            pkg->key = NULL;
        }

        // Move pointer behind key
        pkg_mem += pkg->key_len;

        // Move value into packet
        if (pkg->value_len) {
            if (do_memcpy) {
                pkg->value = malloc(pkg->value_len);
                if (!pkg->value) {
                    ERROR("malloc error: %s\n", strerror(errno));
                    return NULL;
                }
                memcpy(pkg->value, pkg_mem, pkg->value_len);
            } else {
                pkg->value = pkg_mem;
            }
        } else {
            pkg->value = NULL;
        }

    } else {
        ERROR("packet struct contains unknown type.\n");
        pkg->type = INVALID_HEADER;
        return NULL;
    }

    return pkg;
}

// Initialize body-struct with 'zero values'
packet init_packet_empty() {
    packet new_pkg;

    new_pkg.type = INVALID_HEADER;

    new_pkg.hash_id   = 0;
    new_pkg.node_id   = 0;
    new_pkg.node_ip   = 0;
    new_pkg.node_port = 0;

    new_pkg.key_len = 0;
    new_pkg.key = NULL;
    new_pkg.value_len = 0;
    new_pkg.value = NULL;

    return new_pkg;
}

packet init_hash_packet(PKG_METHOD_TYPE type, KEY_LEN_TYPE key_len, void* key, VALUE_LEN_TYPE value_len, void* value) {
    packet new_pkg = init_packet_empty();

    if (!is_protocol_compliant(type, key_len, value_len)) {
        ERROR("Packet would not be compliant with protocol.\n");
        return new_pkg;
    } else if (key_len && !key) {
        ERROR("key points to NULL.\n");
        return new_pkg;
    } else if (value_len && !value) {
        ERROR("value points to NULL.\n");
        return new_pkg;
    }

    new_pkg.type = type;

    new_pkg.key_len = key_len;
    new_pkg.key = key;
    new_pkg.value_len = value_len;
    new_pkg.value = value;

    return new_pkg;

}

char* translate_ip(const char* ip) {
    char* translated_ip = malloc(16 * sizeof(char));
    if (strlen(ip) == 9 && !strncmp(ip, "localhost", 9)) {
        char* localhost = "127.0.0.1";
        translated_ip = strncpy(translated_ip, localhost, 10);
    } else {
        translated_ip = strncpy(translated_ip, ip, 16);
    }

    return translated_ip;
}

uint32_t string_to_ip4(const char* ip) {
    uint32_t ip_l = 0;
    char* translated_ip = translate_ip(ip);
    char* p = translated_ip;
    for (int i = 0; i < 4; i++) {
        int j = 0;
        while (p[j] != '.' && p[j] != '\0') {
            j++;
        }
        if (i != 3 && p[j] == '\0') {
            return 0;
        }
        p[j] = '\0';
        uint8_t single_byte = atoi(p);
        p += j + 1;

        ip_l += (single_byte << (3-i)*8);
    }
    free(translated_ip);
    return ip_l;
}
//TODO: \0 at the end of id_s??
char *id_to_string(uint16_t id) { 
    char *id_s = malloc(sizeof(char)*6);
    snprintf(id_s, 6, "%d", id);
    return id_s;
}

void* create_lookup(HASH_ID_TYPE hash_id, const char* id, const char* ip, const char* port) {
    packet lkp;
    lkp.type = LKP;
    lkp.hash_id = hash_id;
    lkp.node_id = atoi(id); // TODO: this is fucking redundant
    lkp.node_ip = string_to_ip4(ip);
    lkp.node_port = atoi(port);
    size_t len;
    return build_packet(lkp, &len);
}

void* create_join(const char *id, const char* ip, const char *port) {
    packet jpack;
    jpack.type = JOINING;
    jpack.hash_id = 0; // Not needed in Join-Message
    jpack.node_id = atoi(id);
    jpack.node_ip = string_to_ip4(ip);
    jpack.node_port = atoi(port);
    size_t len;
    return build_packet(jpack, &len);
}

void* create_stabilize(const char *id, const char* ip, const char *port) {
    packet sbl;
    sbl.type = STAB;
    sbl.hash_id = 0; // Not needed in Stabilize-Message
    sbl.node_id = atoi(id);
    sbl.node_ip = string_to_ip4(ip);
    sbl.node_port = atoi(port);
    size_t len;
    return build_packet(sbl, &len);
}

void* create_notify(NODE_ID_TYPE id, NODE_IP_TYPE ip, NODE_PORT_TYPE port) {
    packet ntf;
    ntf.type = NOTIFY;
    ntf.hash_id = 0; // Not needed in Notify-Message
    ntf.node_id = id;
    ntf.node_ip = ip;
    ntf.node_port = port;
    size_t len;
    return build_packet(ntf, &len);
}

void* create_fingerlkp(HASH_ID_TYPE hash_id, NODE_ID_TYPE id, NODE_IP_TYPE ip, NODE_PORT_TYPE port) {
    packet flkp;
    flkp.type = LKP;
    flkp.hash_id = hash_id;
    flkp.node_id = id;
    flkp.node_ip = ip;
    flkp.node_port = port;
    size_t len;
    return build_packet(flkp, &len);
}

void* create_f_ack() {
    packet f_ack;
    f_ack.type = F_ACK;
    f_ack.hash_id = 0;
    f_ack.node_id = 0;
    f_ack.node_ip = 0;
    f_ack.node_port = 0;
    size_t len;
    return build_packet(f_ack, &len);
}

void* build_packet(packet pkg, size_t* pkg_len) {
    if (!pkg_len) {
        ERROR("pkg_len points to NULL.\n");
        return NULL;
    }
    *pkg_len = 0;
    void* (*specific_builder)(packet pkg, void* packet, size_t* pkg_len);

    // forward building to specific function (+ more error handling)
    if (pkg.type == INVALID_HEADER) {
        ERROR("invalid packet type.\n");
        return NULL;
    } else if (is_known_control_type(pkg.type)) {
        specific_builder = _build_control_packet;
        *pkg_len = CNTRL_LEN;
    } else if (is_known_hash_type(pkg.type)) {
        specific_builder = _build_hash_packet;
        *pkg_len = HEADER_LEN + pkg.key_len + pkg.value_len;
    } else {
        ERROR("packet struct contains unknown type.\n");
        return NULL;
    }

    void* packet = calloc(*pkg_len, 1);
    if (!packet) {
        *pkg_len = 0;
        ERROR("calloc error: %s\n", strerror(errno));
        return NULL;
    }

    return specific_builder(pkg, packet, pkg_len);
}

void* _build_control_packet(packet pkg, void* packet, size_t* pkg_len) {
    size_t len = *pkg_len;
    *pkg_len = 0;
    if (len != CNTRL_LEN) {
        ERROR("Package has not the right size.\n");
        return NULL;
    }
    void* p = packet;
    
    // Copy type into packet
    memcpy(p, &pkg.type, sizeof pkg.type);
    p += sizeof pkg.type;
    
    // Convert hash_id to network-byte-order and copy into packet
    HASH_ID_TYPE net_hash_id = HTON(HASH_ID)(pkg.hash_id);
    memcpy(p, &net_hash_id, sizeof net_hash_id);
    p += sizeof net_hash_id;

    // Convert node_id to network-byte-order and copy into packet
    NODE_ID_TYPE net_node_id = HTON(NODE_ID)(pkg.node_id);
    memcpy(p, &net_node_id, sizeof net_node_id);
    p += sizeof net_node_id;

    // Convert node_ip to network-byte-order and copy into packet
    NODE_IP_TYPE net_node_ip = HTON(NODE_IP)(pkg.node_ip);
    memcpy(p, &net_node_ip, sizeof net_node_ip);
    p += sizeof net_node_ip;

    // Convert node_port to network-byte-order and copy into packet
    NODE_PORT_TYPE net_node_port = HTON(NODE_PORT)(pkg.node_port);
    memcpy(p, &net_node_port, sizeof net_node_port);
    p += sizeof net_node_port;
    
    *pkg_len = CNTRL_LEN;
    return packet;
}

void* _build_hash_packet(packet pkg, void* packet, size_t* pkg_len) {
    size_t len = *pkg_len;
    *pkg_len = 0;
    if (len != HEADER_LEN + pkg.key_len + pkg.value_len) {
        ERROR("Package has not the right size.\n");
        return NULL;
    }

    // Check if packet would be compliant with protocol specs and if pointers are valid
    if (!is_protocol_compliant(pkg.type, pkg.key_len, pkg.value_len)) {
        // err msg printed already
        return NULL;
    } else if (pkg.key_len && !pkg.key) {
        ERROR("key points to NULL.\n");
        return NULL;
    } else if (pkg.value_len && !pkg.value) {
        ERROR("value points to NULL.\n");
        return NULL;
    }

    void* p = packet;

    // Copy type into packet
    memcpy(p, &pkg.type, sizeof pkg.type);
    p += sizeof pkg.type;

    // Convert key_len to network-byte-order and copy into packet
    KEY_LEN_TYPE net_key_len = HTON(KEY)(pkg.key_len);
    memcpy(p, &net_key_len, sizeof net_key_len);
    p += sizeof net_key_len;

    // Convert value_len to network-byte-order and copy into packet
    VALUE_LEN_TYPE net_value_len = HTON(VALUE)(pkg.value_len);
    memcpy(p, &net_value_len, sizeof net_value_len);
    p += sizeof net_value_len;

    // Copy key into packet
    if (pkg.key_len) {
        memcpy(p, pkg.key, pkg.key_len);
        p += pkg.key_len;
    }

    // Copy value into packet
    if (pkg.value_len) {
        memcpy(p, pkg.value, pkg.value_len);
        p += pkg.value_len;
    }

    // Set final length and return;
    *pkg_len = p - packet;
    return packet;
}

int is_protocol_compliant(PKG_METHOD_TYPE pkg_type, KEY_LEN_TYPE key_len, VALUE_LEN_TYPE value_len) {
    switch (pkg_type) {
        case SET_REQ:
        case GET_REQ:
        case DEL_REQ:
            if (!key_len) {
                ERROR("All requests need nonempty keys.\n");
                return 0;
            } else if (pkg_type != SET_REQ && value_len) {
                // If values were allowed and just ignored by the server it could lead to misstyped requests being executed (consequences for DELETE)
                ERROR("GET- and DELETE-requests need empty values.\n");
                return 0;
            }
            break;
        case GET_ACK:
            // empty value         -> key existst but has empty value
            // empty key and value -> key not found
            if (!key_len && value_len) {
                ERROR("SET-requests can't send values without a key.\n");
                return 0;
            }
            break;
        case SET_ACK:
        case DEL_ACK:
            if (key_len || value_len) {
                ERROR("SET- and DELETE-acknowledments need empty keys and values.\n");
                return 0;
            }
            break;
        default:
            ERROR("Unknown type.\n");
            return 0;
    }
    return 1;
}

void printInfo(void* stream, packet pkg) {
    fprintf(stream, "Decoded packet header:\n");
    fprintf(stream, "ACK: %u\n", !!(pkg.type & ACK));
    fprintf(stream, "GET: %u\n", !!(pkg.type & GET));
    fprintf(stream, "SET: %u\n", !!(pkg.type & SET));
    fprintf(stream, "DEL: %u\n", !!(pkg.type & DEL));
    fprintf(stream, "Key Length: %u Bytes\n", pkg.key_len);
    fprintf(stream, "Value Length: %u Bytes\n", pkg.value_len);
}

/*
 * Return codes:
 *      - 0: if it is the own range
 *      - -1: if it is not the own range
 */
int is_own_range__notify(peer *self, uint16_t id) {
    if(self->node_id < self->succ->node_id) {
        //Normal case
        if(self->succ->node_id > id && id > self->node_id) {
            return 0;
        }
    } else {
        //Special case
        if(self->succ->node_id > id || id > self->node_id) {
            return 0;
        }
    }
    return -1;
}

int is_own_range__join(peer *self, uint16_t id) {
    if(self->pred->node_port == 0) return -1;
    
    if(self->node_id < self->pred->node_id) {
        //Special case
        if(id < self->node_id || id > self->pred->node_id) {
            return 0;
        }
    } else {
        //Normal case
        if(id < self->node_id && id > self->pred->node_id) {
            return 0;
        }
    }
    return -1;
}

int is_own_range__stabilize(peer *self, uint16_t id) {
    if(self->node_id < self->pred->node_id) {
        //Special case
        if(id < self->node_id || id > self->pred->node_id) {
            return 0;
        }
    } else {
        //Normal case
        if(id < self->node_id && id > self->pred->node_id) {
            return 0;
        }
    }
    
    return -1;
}
