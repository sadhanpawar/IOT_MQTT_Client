// TCP Library (framework only)
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

#ifndef TCP_H_
#define TCP_H_

#include <stdint.h>
#include <stdbool.h>
#include "ip.h"

typedef struct _tcpHeader // 20 or more bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint32_t sequenceNumber;
  uint32_t acknowledgementNumber;
  uint16_t offsetFields;
  uint16_t windowSize;
  uint16_t checksum;
  uint16_t urgentPointer;
  uint8_t  data[0];
} tcpHeader;

// TCP states
#define TCP_CLOSED 0
#define TCP_LISTEN 1
#define TCP_SYN_RECEIVED 2
#define TCP_SYN_SENT 3
#define TCP_ESTABLISHED 4
#define TCP_FIN_WAIT_1 5
#define TCP_FIN_WAIT_2 6
#define TCP_CLOSING 7
#define TCP_CLOSE_WAIT 8
#define TCP_LAST_ACK 9
#define TCP_TIME_WAIT 10

// TCP offset/flags
#define FIN 0x0001
#define SYN 0x0002
#define RST 0x0004
#define PSH 0x0008
#define ACK 0x0010
#define URG 0x0020
#define ECE 0x0040
#define CWR 0x0080
#define NS  0x0100
#define OFS_SHIFT 12

#define MSS       (1460)

#define NO_OF_SOCKETS     (1u)
#define LOCAL_PORT        (8000u)
#define REMOTE_PORT       (8181u)
#define REMOTE_HW_MAC     ()

#define SETBIT(x,n)       (x |= (1 << n))
#define CLEARBIT(x,n)     (x &= ~(1 << n))

#define TCP_RX            (1u)
#define TCP_TX            (2u)

#define TCP_INV_VAL       (0xFF)

typedef struct _Tcb
{
  socket  s;
  uint8_t fsmState;
  bool    initConnect;
  uint8_t socketIdx;
}Tcb_t;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void tcpSetState(uint8_t state);
uint8_t tcpGetState();

bool tcpIsSyn(etherHeader *ether);
bool tcpIsAck(etherHeader *ether);

bool isTcp(etherHeader *ether);

// TODO: Add functions here

uint8_t *getTcpData(etherHeader *ether);
void tcpSendSyn(etherHeader *ether);
void tcpFsmStateMachineClient(etherHeader *ether, uint8_t Idx, uint8_t flag);
void tcpHandlerTx(etherHeader *ether);
void tcpHandlerRx(etherHeader *ether);
void tcpCreateSocket(uint16_t remotePort, uint8_t *toIp);
void tcpConnect(uint8_t socketno);
uint8_t tcpGetMatchingSocket(etherHeader *ether);
void tcpSendAck(etherHeader *ether);
void tcpSendFin(etherHeader *ether);
void tcpHandleRwTransactions(etherHeader *ether, uint8_t flag);

#endif

