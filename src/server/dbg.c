#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


#include "server/debug.h"
#include "server/globals.h"
#include "server/protocol.h"
#include "server/util.h"
#include "server/dbg.h"

static uint8_t broadcast_if_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void debug_send_packet(uint8_t *data, size_t len) {
    struct EthernetHeader *out_eh = (struct EthernetHeader *)(g_send_buffer);
    struct ProtocolHeader *out_ph = (struct ProtocolHeader *)(out_eh + 1);
    uint8_t *out_payload = (uint8_t *)(out_ph + 1);
  
    // Prepare the packet
    memcpy(out_eh->dest_mac_addr, broadcast_if_addr, ETH_ALEN);
    memcpy(out_eh->src_mac_addr, g_pktdrv_info.mac_addr, ETH_ALEN);
    out_eh->ethertype = htons(g_ethertype);
  
    out_ph->signature = htonl(PACKET_SIGNATURE);
    out_ph->payload_len = htons(len);
    out_ph->pkt_type = htons(V1_DEBUG);
  
    memcpy(out_payload, data, len);
  
    pktdrv_send(g_send_buffer, COMBINED_HEADER_LEN + len);
}
  
int debugpkt(const char *fmt, ...) {
    va_list ap;    
    char tmp[1024];
    int r;
    va_start(ap, fmt);
    r = vsprintf(tmp, fmt, ap);
    va_end(ap);
    debug_send_packet((uint8_t *)tmp, r);
    return r;
}  

