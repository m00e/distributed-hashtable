#ifndef _JOIN_OP
#define _JOIN_OP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer_struct.h"
#include "protocol.h"
#include "err.h"
#include "net.h"

void *stabilize(void *sending_peer);
void handle_lookup(void* req_mem, packet req, int range, peer *n_peer);
int handle_stabilize(peer *self, packet req);
int handle_notify(peer *self, packet req);
int handle_join(peer *self, packet req, void* request);
#endif
