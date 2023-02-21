// IP Layer Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef IP_CUSTOM_LAYER_H_
#define IP_CUSTOM_LAYER_H_

#include <stdint.h>
#include <stdbool.h>
#include "ip.h"
#include "timer.h"

typedef enum 
{
    IP_NOEVENT,
    IP_ARP_REQ_RESP
}ipEvent_t;

extern ipEvent_t ipEventIcb;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
void ipHandlerTx(etherHeader *ether);
void ipHandlerRx(etherHeader *ether);
void setIpMqttMacBrokerAddress(uint8_t mac[HW_ADD_LENGTH]);
ipEvent_t getIpCurrentEventStatus(void);
_callback arpRespTimeoutCb();
bool isIpArpResponse(etherHeader *ether);
#endif

