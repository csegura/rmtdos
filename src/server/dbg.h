#ifndef __DBG_H__
#define __DBG_H__

#define V1_DEBUG 8

extern void debug_send_packet(uint8_t *data, size_t len);
extern int debugpkt(const char *fmt, ...);

#endif // __DBG_H__