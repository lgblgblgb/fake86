#ifndef FAKE86_NETCARD_H_INCLUDED
#define FAKE86_NETCARD_H_INCLUDED

#include <stdint.h>

extern struct netstruct {                                                                                                                                                                                            
        uint8_t enabled;
        uint8_t canrecv;
        uint16_t pktlen;
} net;

#endif
