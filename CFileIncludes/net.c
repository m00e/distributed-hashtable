#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "../Headers/net.h"
#include "../Headers/err.h"
#include "../Headers/protocol.h" // used in recv_packet()

#define MAX_QUEUE_LEN 10

int build_connection_as_client(const char* node, const char* port) {
    return _build_connection(node, port, 0);
}

int build_connection_as_server(const char* port) {
    return _build_connection(NULL, port, 1);
}

int _build_connection(const char* node, const char* port,  int isServer) {
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (isServer) hints.ai_flags = AI_PASSIVE;
    
    int status = getaddrinfo(node, port, &hints, &servinfo);
    if (status != 0) {
        ERROR("getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    int socketfd = -1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        socketfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        if (socketfd == -1) continue;

        if (isServer) {
            // Beej's guide: "lose the pesky "Address already in use" error message"
            int yes = 1;
            if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
                close(socketfd);
                ERROR("setsockopt error: %s\n", strerror(errno));
                return -1;
            }

            if (bind(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(socketfd);
                continue;
            }
        } else {
            if (connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(socketfd);
                continue;
            }
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (!p) {
        close(socketfd);
        ERROR("failed to %s.\n", (isServer ? "bind" : "connect"));
        return -1;
    }

    if (isServer && listen(socketfd, MAX_QUEUE_LEN) == -1) {
        close(socketfd);
        ERROR("listen error: %s\n", strerror(errno));
        return -1;
    }

    return socketfd;
}

int accept_connection(int socketfd) {
    struct sockaddr_storage client_addr;
    socklen_t addrlen = sizeof client_addr;
    int new_fd = accept(socketfd, (struct sockaddr*)&client_addr, &addrlen);
    if (new_fd == -1) ERROR("accept error: %s\n", strerror(errno));
    return new_fd;
}

void* recvn(int socketfd, void* buf, size_t len, size_t* offset) {
    // Receive request header
    ssize_t bytes_recv = 0;
    while (*offset < len && (bytes_recv = recv(socketfd, buf + *offset, len - *offset, 0)) > 0) {
        *offset += bytes_recv;
    }
    if (*offset != len) {
        if (bytes_recv == -1) {
            ERROR("recv error: %s\n", strerror(errno));
        } else {
            ERROR("Packet shorter than expected.\n");
        }
        return NULL;
    }
    return buf;
}

void* sendn(int socketfd, void* packet, size_t len, int flags) {
    ssize_t bytes_send = 0;
    size_t bytes_total = 0;
    // TOOD: move pointer if while runs more than once, similar to recvn
    while (bytes_total < len && (bytes_send = send(socketfd, packet, len, flags)) > 0) {
        bytes_total += bytes_send;
    }
    if (bytes_total != len) {
        if (bytes_send == -1) {
            ERROR("send error: %s\n", strerror(errno));
        } else {
            ERROR("Given packet was not transmitted fully.\n");
        }
        return NULL;
    }
    return packet;
}

void* recv_packet(int socketfd, packet* pkg, size_t* pkg_size) {
    // Allocate memory for packet;
    void* buf = malloc(HEADER_LEN);
    if (!buf) {
        ERROR("malloc error: %s\n", strerror(errno));
        return NULL;
    }
    *pkg_size = HEADER_LEN;

    // Receive header
    size_t offset = 0;
    if (!recvn(socketfd, buf, *pkg_size, &offset)) {
        // error message printed already
        free(buf);
        return NULL;
    }
    DEBUG("Header received.\n");
    
    // initialize header struct from header memory
    *pkg = extract_header(buf, 1);
    char method_name[13];
    if (pkg->type == INVALID_HEADER || !bits_to_method_name(pkg->type, method_name, 13)) {
        // error message was already printed
        free(buf);
        return NULL;
    }
    DEBUG("Received: %s\n", method_name);
    if (is_known_control_type(pkg->type)) {
        // Receive bits that are still missing
        *pkg_size += CNTRL_LEN - *pkg_size;
    } else if (is_known_hash_type(pkg->type)) {
        // Receive expected key and value
        *pkg_size += pkg->key_len + pkg->value_len;
    }

    // Reallocate memory for body
    buf = realloc(buf, *pkg_size);
    if (!buf) {
        ERROR("realloc error: %s\n", strerror(errno));
        free(buf);
        return NULL;
    }
    
    // Receive body
    if (!recvn(socketfd, buf, *pkg_size, &offset)) {
        // error message printed already
        free(buf);
        return NULL;
    }
    
    return buf;
}

// Sends lookup to next peer
int ask_next(void* lookup_pkg, const char* ip, const char* port) {
    if (!lookup_pkg) {
        ERROR("Lookup packet points to NULL.\n");
        return 0;
    }
    int socketfd;
    if ((socketfd = build_connection_as_client(ip, port)) == -1) {
        // error message printed already
        return 0;
    }
    if (!sendn(socketfd, lookup_pkg, CNTRL_LEN, 0)) {
        // error message printed already
        close(socketfd);
        return 0;
    }
    close(socketfd);
    return 1;
}

int forward_packet(char* ip, char* port, void* pkg, size_t len) {
    int socketfd;
    if (-1 == (socketfd = build_connection_as_client(ip, port))) {
        return -1;
    }
    if (!sendn(socketfd, pkg, len, 0)) {
        close(socketfd);
        return -1;
    }
    DEBUG("Packet forwarded.\n");
    return socketfd;
}

// Writes given ip into a string (needs to be freed)
char* ip4_to_string(uint32_t ip) {
    // An ip has (with zero padding and dots between bytes) 15 digits (+ \0)
    char* ip_s = malloc(16 * sizeof(char));
    // init bit mask of one byte
    uint32_t b = 0xFF;
    for (int i = 0; i < 4; i++) {
        // use byte-mask and shift that result so that it fits into 8 bit
        uint8_t single_byte = (ip & (b << (3-i)*8)) >> (3-i)*8;
        // 'print' that number into our string with padding of 3 digits with zeros
        snprintf(ip_s + i*4, 4, "%03d", single_byte);
        // write a dot between the bytes
        ip_s[(i+1)*4 - 1] = '.';
    }
    // end the string with a null byte
    ip_s[15] = '\0';

    return ip_s;
}

char* port_to_string(uint16_t port) {
    // highest possible port is 2^16 -1. In the decimal system that number occupies 5 digits (+ \0)
    char* port_s = malloc(6 * sizeof(char));
    // 'print' port into string
    snprintf(port_s, 6, "%d", port);

    return port_s;
}
