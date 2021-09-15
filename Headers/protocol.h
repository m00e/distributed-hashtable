#ifndef BLOCK3_PROTOCOL_H
#define BLOCK3_PROTOCOL_H

#include <stddef.h> // for size_t
#include <stdint.h> // for uintn_t

#include "peer_struct.h"

#define BUILD_UINT_TYPE(bits) JOIN3(uint, bits, _t)
#define BUILD_UINT_MAX(bits)  JOIN3(UINT, bits, _MAX)
#define JOIN3(a, b, c) a ## b ## c

#define JOIN(...) PRIMITIVE_JOIN(__VA_ARGS__)
#define PRIMITIVE_JOIN(a, ...) a ## __VA_ARGS__

#define NTOH(var) JOIN(ntoh, SHORT_OR_LONG(var))
#define HTON(var) JOIN(hton, SHORT_OR_LONG(var))
#define SHORT_OR_LONG(var) JOIN(SHORT_OR_LONG_, JOIN(var, _LEN_BITS))

#define SHORT_OR_LONG_16 s
#define SHORT_OR_LONG_32 l

#define PKG_METHOD_LEN_BITS 8
#define PKG_METHOD_LEN      (PKG_METHOD_LEN_BITS / 8)
#define PKG_METHOD_TYPE     BUILD_UINT_TYPE(PKG_METHOD_LEN_BITS)

// control messages
#define CNTRL ((0b1 << 7) | (RESERVED_BITS >> 1))
#define CNTRL_RPLY 0b10
#define LKP        (CNTRL | 0b01)
#define LKP_RPLY   (CNTRL | CNTRL_RPLY)
#define STAB       (CNTRL | 0b100)
#define NOTIFY     (CNTRL | 0b1000)
#define JOINING    (CNTRL | 0b10000)
#define F_ACK      (CNTRL | 0b100000)
#define FINGER     (CNTRL | 0b1000000)
#define F_LKP      (FINGER | LKP) // Not in use, dont delete because we can
#define F_LKP_RPLY (FINGER | LKP_RPLY)

#define HASH_ID_LEN_BITS    16
#define HASH_ID_LEN         (HASH_ID_LEN_BITS / 8)
#define HASH_ID_TYPE        BUILD_UINT_TYPE(HASH_ID_LEN_BITS)
#define HASH_ID_MAX         BUILD_UINT_MAX(HASH_ID_LEN_BITS)
#define HASH_ID_OFFSET      1

#define NODE_ID_LEN_BITS    16
#define NODE_ID_LEN         (NODE_ID_LEN_BITS / 8)
#define NODE_ID_TYPE        BUILD_UINT_TYPE(NODE_ID_LEN_BITS)
#define NODE_ID_MAX         BUILD_UINT_MAX(NODE_ID_LEN_BITS)
#define NODE_ID_OFFSET      (HASH_ID_OFFSET + HASH_ID_LEN)

#define NODE_IP_LEN_BITS    32
#define NODE_IP_LEN         (NODE_IP_LEN_BITS / 8)
#define NODE_IP_TYPE        BUILD_UINT_TYPE(NODE_IP_LEN_BITS)
#define NODE_IP_MAX         BUILD_UINT_MAX(NODE_IP_LEN_BITS)
#define NODE_IP_OFFSET      (NODE_ID_OFFSET + NODE_ID_LEN)

#define NODE_PORT_LEN_BITS  16
#define NODE_PORT_LEN       (NODE_PORT_LEN_BITS / 8)
#define NODE_PORT_TYPE      BUILD_UINT_TYPE(NODE_PORT_LEN_BITS)
#define NODE_PORT_MAX       BUILD_UINT_MAX(NODE_PORT_LEN_BITS)
#define NODE_PORT_OFFSET    (NODE_IP_OFFSET + NODE_IP_LEN)

#define CNTRL_LEN (PKG_METHOD_LEN + HASH_ID_LEN + NODE_ID_LEN + NODE_IP_LEN + NODE_PORT_LEN)

// hash table messages
#define RESERVED_BITS_UNSHIFTED 0b0000
#define RESERVED_BITS (RESERVED_BITS_UNSHIFTED << 4)

#define GET 0b0100
#define SET 0b0010
#define DEL 0b0001

#define GET_REQ (GET | RESERVED_BITS)
#define SET_REQ (SET | RESERVED_BITS)
#define DEL_REQ (DEL | RESERVED_BITS)

#define ACK     0b1000
#define GET_ACK (GET_REQ | ACK)
#define SET_ACK (SET_REQ | ACK)
#define DEL_ACK (DEL_REQ | ACK)

#define KEY_LEN_BITS     16
#define KEY_LEN          (KEY_LEN_BITS / 8)
#define KEY_LEN_TYPE     BUILD_UINT_TYPE(KEY_LEN_BITS)
#define KEY_LEN_MAX      BUILD_UINT_MAX(KEY_LEN_BITS)
#define KEY_LEN_OFFSET   PKG_METHOD_LEN

#define VALUE_LEN_BITS   32
#define VALUE_LEN        (VALUE_LEN_BITS / 8)
#define VALUE_LEN_TYPE   BUILD_UINT_TYPE(VALUE_LEN_BITS)
#define VALUE_LEN_MAX    BUILD_UINT_MAX(VALUE_LEN_BITS)
#define VALUE_LEN_OFFSET (KEY_LEN_OFFSET + KEY_LEN)

#define HEADER_LEN (PKG_METHOD_LEN + KEY_LEN + VALUE_LEN)
#define INVALID_HEADER 0

#define MAX_ID 65536

typedef struct _packet {
    PKG_METHOD_TYPE type;

    union {
        // control
        struct {
            HASH_ID_TYPE hash_id;
            NODE_ID_TYPE node_id;
            NODE_IP_TYPE node_ip;
            NODE_PORT_TYPE node_port;
        };

        // hash table
        struct {
            KEY_LEN_TYPE key_len;
            VALUE_LEN_TYPE value_len;
            void* key;
            void* value;
        };
    };
} packet;

int is_known_type(PKG_METHOD_TYPE pkg_type);
int is_known_control_type(PKG_METHOD_TYPE pkg_type);
int is_known_control_req(PKG_METHOD_TYPE pkg_type);
int is_known_control_rsp(PKG_METHOD_TYPE pkg_type);
int is_known_hash_type(PKG_METHOD_TYPE pkg_type);
int is_known_req_type(PKG_METHOD_TYPE pkg_type);
int is_known_ack_type(PKG_METHOD_TYPE pkg_type);
PKG_METHOD_TYPE method_name_to_bits(const char* type_name);
char* bits_to_method_name(PKG_METHOD_TYPE type, char* name, size_t n);
packet extract_header(void* pkg_header, int check_compliance);
packet* extract_body(void* pkg_mem, packet* pkg, int do_memcpy);
packet init_packet_empty();
packet init_hash_packet(PKG_METHOD_TYPE type, KEY_LEN_TYPE key_len, void* key, VALUE_LEN_TYPE value_len, void* value);
char* translate_ip(const char* ip);
uint32_t string_to_ip4(const char* ip);
char* id_to_string(uint16_t id);
void* create_lookup(HASH_ID_TYPE hash_id, const char* id, const char* ip, const char* port);
void* create_join(const char *id, const char* ip, const char *port);
void* create_stabilize(const char *id, const char* ip, const char *port);
void* create_notify(NODE_ID_TYPE id, NODE_IP_TYPE ip, NODE_PORT_TYPE port);
void* create_fingerlkp(HASH_ID_TYPE hash_id, NODE_ID_TYPE id, NODE_IP_TYPE ip, NODE_PORT_TYPE port);
void* create_f_ack();
void* build_packet(packet pkg, size_t* pkg_len);
void* _build_control_packet(packet pkg, void* packet, size_t* pkg_len);
void* _build_hash_packet(packet pkg, void* packet, size_t* pkg_len);
int is_protocol_compliant(PKG_METHOD_TYPE pkg_type, KEY_LEN_TYPE key_len, VALUE_LEN_TYPE value_len);
void printInfo(void* stream, packet pkg);
int is_own_range__notify(peer *self, uint16_t id);
int is_own_range__join(peer *self, uint16_t id);
int is_own_range__stabilize(peer *self, uint16_t id);
#endif //BLOCK3_PROTOCOL_H
