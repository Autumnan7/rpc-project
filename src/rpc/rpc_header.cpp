#include "rpc/rpc_header.h"
#include <arpa/inet.h>

static const uint16_t DefaultMagic = 0x7777;

void set_rpc_header(void *header, size_t msg_len) {
    ((RpcHeader *) header)->magic = DefaultMagic;
    ((RpcHeader *) header)->len = htonl(msg_len);
}

