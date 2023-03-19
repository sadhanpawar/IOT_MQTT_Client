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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "tcp.h"
#include "timer.h"
#include "mqtt.h"
#include "ip_custom_layer.h"
#include "uart0.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
bool enableRetransFlag = true;
static uint8_t tcpTimerFlag = 0;
static uint8_t tcpTimerIdx = 1;
Tcb_t socketConns[NO_OF_SOCKETS] = {0};
static uint32_t initialAckNo = 0;
static uint32_t initialSeqNo = 0;
bool initiateFin = false;
static uint16_t tcpCurrWindow = 0;
static uint8_t tcpWriteFSM = 0;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------
static char *tcpState[] = {
    "TCP_CLOSED",
    "TCP_LISTEN",
    "TCP_SYN_RECEIVED",
    "TCP_SYN_SENT",
    "TCP_ESTABLISHED",
    "TCP_FIN_WAIT_1",
    "TCP_FIN_WAIT_2",
    "TCP_CLOSING",
    "TCP_CLOSE_WAIT",
    "TCP_LAST_ACK",
    "TCP_TIME_WAIT"
};
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Determines whether packet is TCP packet
// Must be an IP packet

/**
 * @brief checks wether it's Tcp packet
 * 
 * @param ether 
 * @return true 
 * @return false 
 */
bool isTcp(etherHeader* ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    uint32_t sum = 0;
    bool ok;
    uint16_t tmp16;
    ok = (ip->protocol == PROTOCOL_TCP);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sumIpWords(ip->sourceIp, 8, &sum);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        tmp16 = htons(ntohs(ip->length) - (ip->size * 4));
        sumIpWords(&tmp16, 2, &sum);
        // add tcp header and data
        sumIpWords(tcp, ntohs(ip->length) - (ip->size * 4), &sum);
        ok = (getIpChecksum(sum) == 0);
    }
    return ok;
}

// TODO: Add functions here
/***************************************************RX*******************************/
/**
 * @brief Get the Tcp Data object
 * 
 * @param ether 
 * @return uint8_t* 
 */
uint8_t *getTcpData(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    return tcp->data;
}

/**
 * @brief Get the Tcp Header object
 * 
 * @param ether 
 * @return uint8_t* 
 */
uint8_t *getTcpHeader(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    return (uint8_t*)tcp;
}

/**
 * @brief 
 * 
 * @param ether 
 * @return true 
 * @return false 
 */
bool tcpIsSyn(etherHeader *ether)
{
    tcpHeader *tcpHdr = (tcpHeader*)getTcpHeader(ether);
    return ((htons(tcpHdr->offsetFields) & 0x0002) >> 1);

}

/**
 * @brief tcpIsAck
 * 
 * @param ether 
 * @return true 
 * @return false 
 */
bool tcpIsAck(etherHeader *ether)
{
    tcpHeader *tcpHdr = (tcpHeader*)getTcpHeader(ether);
    return ((htons(tcpHdr->offsetFields) & 0x0010) >> 4);

}

/* header lenght is 4 bits in offset field */
/**
 * @brief tcpHeaderSize
 * 
 * @param ether 
 * @return uint8_t 
 */
uint8_t tcpHeaderSize(etherHeader *ether)
{
    tcpHeader *tcpHdr = (tcpHeader*)getTcpHeader(ether);
    return (htons(tcpHdr->offsetFields) >> (OFS_SHIFT));
}

/***************************************************RX*******************************/

/**
 * @brief genRandNum
 * 
 * @return uint32_t 
 */
uint32_t genRandNum(void)
{
    srand(rand());

    return (rand()*10*3);
}

/**
 * @brief tcpHandlerRx
 * 
 * @param ether 
 */
void tcpHandlerRx(etherHeader *ether)
{
    uint8_t socketNo = 0;

    /* check which socket's ip it belongs */
    if( (socketNo = tcpGetMatchingSocket(ether)) != TCP_INV_VAL ) {
        tcpFsmStateMachineClient(ether, socketNo, TCP_RX);
    }

}

/**
 * @brief tcpGetMatchingSocket
 * 
 * @param ether 
 * @return uint8_t 
 */
uint8_t tcpGetMatchingSocket(etherHeader *ether)
{
    uint8_t i;
    uint8_t j;
    ipHeader* ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)getTcpHeader(ether);
    uint8_t localIp[4];

    getIpAddress(localIp);

    for(i = 0; i < NO_OF_SOCKETS; i++) {

        if( (socketConns[i].s.localPort == htons(tcp->destPort))
        )
        {
            for(j = 0; j < IP_ADD_LENGTH; j++){
                if(localIp[j] == ip->destIp[j])
                {
                    /*continue*/
                }
                else
                {
                    break;
                }
            }
            if(j == IP_ADD_LENGTH) {
                return i;
            }   
        }
    }

    return TCP_INV_VAL;
}

/***************************************************TX*******************************/
/**
 * @brief tcpHandlerTx
 * 
 * @param ether 
 */
void tcpHandlerTx(etherHeader *ether)
{
    uint8_t count = 0;

    /* handling the tx data */
    for(count = 0; count < NO_OF_SOCKETS; count++) {
        socketConns[count].socketIdx = count;
        tcpFsmStateMachineClient(ether,count, TCP_TX);
    }

}

/**
 * @brief tcpSendTimerCb
 * 
 * @return _callback 
 */
_callback tcpSendTimerCb()
{
    tcpTimerFlag = 1;
}

/**
 * @brief tcpFsmStateMachineClient
 * 
 * @param ether 
 * @param Idx 
 * @param flag 
 */
void tcpFsmStateMachineClient(etherHeader *ether, uint8_t Idx, uint8_t flag)
{
    char str[50];
    static uint32_t counter = 0;
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);

    switch(socketConns[Idx].fsmState)
    {
        case TCP_CLOSED:
        {
            if(true == socketConns[Idx].initConnect) {
                
                /* recevied arp response */
                if(IP_NOEVENT == getIpCurrentEventStatus()) {
                    tcpSendSyn(ether);
                    socketConns[Idx].fsmState = TCP_SYN_SENT;
                    socketConns[Idx].initConnect = false;
                    
                    if(enableRetransFlag)
                    {
                        tcpTimerFlag = 0;
                        tcpTimerIdx = 1;
                        stopTimer(tcpSendTimerCb);
                        startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                        ++tcpTimerIdx;
                    }
                    counter = random32();
                }
            } else {
                /* wait till you get arp response */  
            }
   
        }break;

        case TCP_SYN_SENT:
        {
            /* awaits syn+ack from servers and sends an ack and enters established */
            if( (flag == TCP_RX) && (htons(tcp->offsetFields) & ACK ) 
                 && (htons(tcp->offsetFields) & SYN)
            )
            {  
                /*send ack */
                socketConns[0].s.acknowledgementNumber = htonl(tcp->acknowledgementNumber);
                socketConns[0].s.sequenceNumber = htonl(tcp->sequenceNumber);
                socketConns[0].tcpSegLen = getTcpSegmentLength(ether);
                tcpCheckOptionsField(ether);
                initialSeqNo += 1;
                tcpSendAck(ether, 1);
                socketConns[0].s.sequenceNumber += 1;
                socketConns[Idx].fsmState = TCP_ESTABLISHED;
                snprintf(str, sizeof(str), "\nTCP STATE: ESTABLISHED \n");
                putsUart0(str);
                counter = 0;
                //socketConns[Idx].initConnect = false;
                stopTimer(tcpSendTimerCb);
            }
            else if(tcpTimerFlag == 1)
            {
                /*re-send */
                if(tcpTimerIdx < 6) {
                    tcpSendSyn(ether);
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    snprintf(str, sizeof(str), "\nTCP STATE: RE_TRANS at %"PRIu8" secs\n",2*(tcpTimerIdx-1));
                    putsUart0(str);
                    ++tcpTimerIdx;
                } else {
                    stopTimer(tcpSendTimerCb);
                    socketConns[Idx].fsmState = TCP_CLOSED;
                }
                tcpTimerFlag = 0;
            }
            else {
                if( counter <  random32() ) {
                    snprintf(str, sizeof(str), ".");
                    putsUart0(str);
                    counter = random32();
                }
            }
            
        }break;

        case TCP_ESTABLISHED:
        {
            /* received fin ? */
            if((flag == TCP_RX) && (htons(tcp->offsetFields) & FIN))
            {
                socketConns[0].s.acknowledgementNumber = htonl(tcp->acknowledgementNumber);
                socketConns[0].s.sequenceNumber = htonl(tcp->sequenceNumber);
                socketConns[0].tcpSegLen = getTcpSegmentLength(ether);
                /*get length of packet to add it to seq no for ack no*/
                socketConns[0].s.sequenceNumber += 1;
                tcpSendAck(ether,0);
                socketConns[Idx].fsmState = TCP_CLOSE_WAIT;
                snprintf(str, sizeof(str), "TCP STATE: FIN ISSUED TCP_CLOSE_WAIT\n");
                putsUart0(str);
                mqttSetConnState(MQTT_DISCONNECTED);
                mqttSetCurrState(NOEVENT);
                if(enableRetransFlag)
                {
                    tcpTimerFlag = 0;
                    tcpTimerIdx = 1;
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    ++tcpTimerIdx;
                }
            }
            else if( (flag == TCP_RX) && (htons(tcp->offsetFields) & RST) )
            {
                socketConns[0].s.acknowledgementNumber = htonl(tcp->acknowledgementNumber);
                socketConns[0].s.sequenceNumber = htonl(tcp->sequenceNumber);
                socketConns[0].tcpSegLen = getTcpSegmentLength(ether);
                tcpSendAck(ether,0);
                socketConns[Idx].fsmState = TCP_CLOSED;
                snprintf(str, sizeof(str), "TCP STATE: RST ISSUED TCP_CLOSED\n");
                putsUart0(str);
                mqttSetConnState(MQTT_DISCONNECTED);
                mqttSetCurrState(NOEVENT);
            }
            else if (initiateFin)
            {
                uint8_t data[MQTT_SIZE] = {0};
                uint16_t size;  

                mqttGetTxData(data, &size);
                tcpSendSegment(ether, data, size,PSH|FIN, 0);
                //tcpSendFin(ether);
                socketConns[Idx].fsmState = TCP_FIN_WAIT_1;
                snprintf(str, sizeof(str), "TCP STATE: FIN START TCP_FIN_WAIT_1\n");
                putsUart0(str);
                initiateFin = false;
                mqttSetConnState(MQTT_DISCONNECTED);
                mqttSetCurrState(NOEVENT);
                counter = random32();

                if(enableRetransFlag)
                {
                    tcpTimerFlag = 0;
                    tcpTimerIdx = 1;
                    stopTimer(tcpSendTimerCb);
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    ++tcpTimerIdx;
                }
                
            }
            else {
                tcpHandleRwTransactions(ether, flag);
            }

        }break;

        case TCP_FIN_WAIT_1:
        {
            if((flag == TCP_RX) && (htons(tcp->offsetFields) & FIN ) )
            {
                initialSeqNo += 1;
                socketConns[0].s.sequenceNumber += 1;
                tcpSendAck(ether,0);
                socketConns[Idx].fsmState = TCP_CLOSING;
                snprintf(str, sizeof(str), "TCP STATE: FIN START TCP_CLOSING\n");
                putsUart0(str);
                counter = random32();

                if(enableRetransFlag)
                {
                    stopTimer(tcpSendTimerCb);
                    tcpTimerFlag = 0;
                    tcpTimerIdx = 1;
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    ++tcpTimerIdx;
                }
            }
            else if ( (flag == TCP_RX) && (htons(tcp->offsetFields) & ACK ) )
            {
                socketConns[Idx].fsmState = TCP_FIN_WAIT_2;
                snprintf(str, sizeof(str), "TCP STATE: FIN START TCP_FIN_WAIT_2\n");
                putsUart0(str);
                counter = random32();

                if(enableRetransFlag)
                {
                    stopTimer(tcpSendTimerCb);
                    tcpTimerFlag = 0;
                    tcpTimerIdx = 1;
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    ++tcpTimerIdx;
                }

            }
            else if(tcpTimerFlag == 1)
            {
                /*re-send */
                if(tcpTimerIdx < 6) {
                    tcpSendFin(ether);
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    snprintf(str, sizeof(str), "\nTCP STATE: RE_TRANS at %"PRIu8" secs\n",2*(tcpTimerIdx-1));
                    putsUart0(str);
                    ++tcpTimerIdx;
                } else {
                    stopTimer(tcpSendTimerCb);
                    socketConns[Idx].fsmState = TCP_CLOSED;
                }
                tcpTimerFlag = 0;
            }
            else
            {
                if( counter <  random32() ) {
                    snprintf(str, sizeof(str), ".");
                    putsUart0(str);
                    counter = random32();
                }
            }

        }break;

        case TCP_FIN_WAIT_2:
        {
            if((flag == TCP_RX) && (htons(tcp->offsetFields) & FIN) )
            {
                initialSeqNo += 1;
                socketConns[0].s.sequenceNumber += 1;
                tcpSendAck(ether,0);
                socketConns[Idx].fsmState = TCP_TIME_WAIT;
                snprintf(str, sizeof(str), "TCP STATE: FIN START TCP_TIME_WAIT\n");
                putsUart0(str);
                stopTimer(tcpSendTimerCb);
            }
            else if(tcpTimerFlag == 1)
            {
                /*re-send */
                if(tcpTimerIdx < 6) {
                    tcpSendFin(ether);
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    snprintf(str, sizeof(str), "\nTCP STATE: RE_TRANS at %"PRIu8" secs\n",2*(tcpTimerIdx-1));
                    putsUart0(str);
                    ++tcpTimerIdx;
                } else {
                    stopTimer(tcpSendTimerCb);
                    socketConns[Idx].fsmState = TCP_CLOSED;
                }
                tcpTimerFlag = 0;
            }
            else
            {
                if( counter <  random32() ) {
                    snprintf(str, sizeof(str), ".");
                    putsUart0(str);
                    counter = random32();
                }
            }

        }break;

        case TCP_CLOSING:
        {
            if((flag == TCP_RX) && (htons(tcp->offsetFields) & ACK))
            {
                socketConns[Idx].fsmState = TCP_TIME_WAIT;
                snprintf(str, sizeof(str), "TCP STATE: FIN START TCP_TIME_WAIT\n");
                putsUart0(str);
                stopTimer(tcpSendTimerCb);
            }
            else if((flag == TCP_RX) && (htons(tcp->offsetFields) & RST))
            {
                socketConns[Idx].fsmState = TCP_TIME_WAIT;
                snprintf(str, sizeof(str), "TCP STATE: TCP_CLOSED\n");
                putsUart0(str);
                stopTimer(tcpSendTimerCb);
                socketConns[Idx].fsmState = TCP_CLOSED;
            }
            else if(tcpTimerFlag == 1)
            {
                /*re-send */
                if(tcpTimerIdx < 6) {
                    //tcpSendAck(ether,0);
                    tcpSendFin(ether);
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    snprintf(str, sizeof(str), "\nTCP STATE: RE_TRANS at %"PRIu8" secs\n",2*(tcpTimerIdx-1));
                    putsUart0(str);
                    ++tcpTimerIdx;
                } else {
                    stopTimer(tcpSendTimerCb);
                    socketConns[Idx].fsmState = TCP_CLOSED;
                }
                tcpTimerFlag = 0;
            }
            else
            {
                if( counter <  random32() ) {
                    snprintf(str, sizeof(str), ".");
                    putsUart0(str);
                    counter = random32();
                }
            }

        }break;

        case TCP_CLOSE_WAIT:
        {
            
            tcpSendFin(ether);
            socketConns[Idx].fsmState = TCP_LAST_ACK;
            snprintf(str, sizeof(str), "TCP STATE: TCP_LAST_ACK\n");
            putsUart0(str);

            if(enableRetransFlag)
            {
                stopTimer(tcpSendTimerCb);
                tcpTimerFlag = 0;
                tcpTimerIdx = 1;
                startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                ++tcpTimerIdx;
            }

        }break;

        case TCP_LAST_ACK:
        {
            if((flag == TCP_RX) && (htons(tcp->offsetFields) & ACK) )
            {
                socketConns[Idx].fsmState = TCP_CLOSED;
                snprintf(str, sizeof(str), "TCP STATE: TCP_CLOSED\n");
                putsUart0(str);
                stopTimer(tcpSendTimerCb);
            }
            else if(tcpTimerFlag == 1)
            {
                /*re-send */
                if(tcpTimerIdx < 6) {
                    //tcpSendAck(ether,0);
                    tcpSendFin(ether);
                    startOneshotTimer(tcpSendTimerCb,2*tcpTimerIdx);
                    snprintf(str, sizeof(str), "\nTCP STATE: RE_TRANS at %"PRIu8" secs\n",2*(tcpTimerIdx-1));
                    putsUart0(str);
                    ++tcpTimerIdx;
                } else {
                    stopTimer(tcpSendTimerCb);
                    socketConns[Idx].fsmState = TCP_CLOSED;
                }
                tcpTimerFlag = 0;
            }

        }break;

        case TCP_TIME_WAIT:
        {
            socketConns[Idx].fsmState = TCP_CLOSED;
            snprintf(str, sizeof(str), "TCP STATE: TCP_CLOSED\n");
            putsUart0(str);
            
        }break;

        case TCP_LISTEN:
        case TCP_SYN_RECEIVED:
            /* for server */
        default: break;
    }
}

/* function needs to be set to initate tx */

/**
 * @brief tcpConnect
 * 
 * @param socketno 
 */
void tcpConnect(uint8_t socketno)
{
    /* set the socket connection to syn sent */
    socketConns[socketno].initConnect = true;
}

/**
 * @brief Get the Tcp Curr State object
 * 
 * @param i 
 * @return uint8_t 
 */
uint8_t getTcpCurrState(uint8_t i)
{
    return socketConns[i].fsmState;
}

/**
 * @brief Get the Tcp Segment Length object
 * 
 * @param ether 
 * @return uint8_t 
 */
uint8_t getTcpSegmentLength(etherHeader *ether)
{
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t len = 0;
    uint8_t tcpHdrLen = (tcpHeaderSize(ether)*4);
    uint16_t ipLen = htons(ip->length);


     
    len = ipLen - sizeof(ipHeader) - tcpHdrLen;

    return len;
}

/**
 * @brief Get the Tcp Message Socket object
 * 
 * @param ether 
 * @param s 
 */
void getTcpMessageSocket(etherHeader *ether, socket *s)
{
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t i;

    for (i = 0; i < HW_ADD_LENGTH; i++)
        s->remoteHwAddress[i] = ether->sourceAddress[i];
    for (i = 0; i < IP_ADD_LENGTH; i++)
        s->remoteIpAddress[i] = ip->sourceIp[i];
    s->remotePort = ntohs(tcp->sourcePort);
    s->localPort = ntohs(tcp->destPort);
}

/**
 * @brief tcpSendSyn
 * 
 * @param ether 
 */
void tcpSendSyn(etherHeader *ether)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint32_t tmp32;
    uint16_t tcpHdrlen;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    //uint8_t *ptr;
    uint8_t ipHeaderLength;
   /* uint8_t optionsField[] = {0x1,0x1,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,
                                0x9,0x10}; {0x2, 0x4, 0x5, 0xB4}; kind, len, size(2bytes)*/

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = socketConns[0].s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    ipHeaderLength = ip->size * 4;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    initialSeqNo = tmp32 = genRandNum();
    tcp->sequenceNumber = htonl(tmp32);
    tcp->acknowledgementNumber = 0;
    tcp->windowSize = 0; /* htons(MSS);  */
    tcp->urgentPointer = 0;
    tcp->offsetFields = 0;
    SETBIT(tcp->offsetFields,SYN);
    
    tcpLength = sizeof(tcpHeader) + /*sizeof(optionsField)  +*/ 0; /*syn has no data */
    
    tcp->offsetFields &= ~(0xF000);
    tcpHdrlen = ((sizeof(tcpHeader)/4) << OFS_SHIFT) ; /* always it's *4 */
    tcp->offsetFields |= tcpHdrlen;

    tcp->offsetFields = htons(tcp->offsetFields);

    /*
    ptr = (uint8_t *)tcp->data;
    for(i = 0; i < sizeof(optionsField); i++) {
        ptr[i] = optionsField[i];
    }
    */

    ip->length = htons(sizeof(ipHeader) + tcpLength) ;
    calcIpChecksum(ip);

    /*tcp checksum:
    * psuedo header + tcp header + tcp data
    */

    /*source & dst addresss */
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);

    /*protocol and reserved */
    tmp16 = ip->protocol;
    sum += ((tmp16 & 0xff) << 8);

    /*tcp length*/
    tmp16 = htons(tcpLength);
    sumIpWords(&tmp16, 2, &sum);

    /*tcp header + tcp data */
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);

    /*load the checksum*/
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

/**
 * @brief tcpSendAck
 * 
 * @param ether 
 * @param ackVal 
 */
void tcpSendAck(etherHeader *ether, uint8_t ackVal)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint16_t tcpHdrlen;
    uint8_t ipHeaderLength;
    /*uint8_t optionsField[] = {0x1,0x1,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,
                                0x9,0x10};*/
    uint8_t *ptr;

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = socketConns[0].s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    ipHeaderLength = ip->size * 4;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header TODO change the tx and rx buffer
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tcp->acknowledgementNumber = htonl(socketConns[0].s.sequenceNumber +
                                        socketConns[0].tcpSegLen + ackVal);
    tcp->sequenceNumber = htonl(initialSeqNo);//tcp->acknowledgementNumber;
    
    //tcp->sequenceNumber = socketConns[0].s.sequenceNumber += 1;
    //tcp->acknowledgementNumber = socketConns[0].s.acknowledgementNumber;
    tcp->windowSize = tcpCurrWindow = tcp->windowSize; //htons(MSS);
    tcp->urgentPointer = 0;
    tcp->offsetFields = 0;
    SETBIT(tcp->offsetFields,ACK);
    
    tcpLength = sizeof(tcpHeader) + /*sizeof(optionsField) +*/ 0; /*ack has no data */
    #if 0
    tcp->offsetFields &= ~(0xF000);
    tcpHdrlen = ((sizeof(tcpHeader)/4) << OFS_SHIFT) ; /* always it's *4 */
    tcp->offsetFields |= tcpHdrlen;
    
    tcp->offsetFields = htons(tcp->offsetFields);
    #endif

    if(socketConns[0].o.optFldFlag) {
        ptr = (uint8_t *)tcp->data;
        for(i = 0; i < socketConns[0].o.optFldLen; i++) {
            ptr[i] = socketConns[0].o.optFldData[i];
        }
        tcpLength += i;
        socketConns[0].o.optFldFlag = false;

        tcp->offsetFields &= ~(0xF000);
        tcpHdrlen = (((sizeof(tcpHeader)/4) + (socketConns[0].o.optFldLen/4)) << OFS_SHIFT) ; /* always it's *4 */
        tcp->offsetFields |= tcpHdrlen;

    } else {
        tcp->offsetFields &= ~(0xF000);
        tcpHdrlen = ((sizeof(tcpHeader)/4) << OFS_SHIFT) ; /* always it's *4 */
        tcp->offsetFields |= tcpHdrlen;
    }
    
    tcp->offsetFields = htons(tcp->offsetFields);
    
    ip->length = htons(sizeof(ipHeader) + tcpLength) ;
    calcIpChecksum(ip);

    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tmp16 = htons(tcpLength);
    sumIpWords(&tmp16, 2, &sum); /* doubt in this */
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

/**
 * @brief tcpSendFin
 * 
 * @param ether 
 */
void tcpSendFin(etherHeader *ether)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint16_t tcpHdrlen;
    uint8_t ipHeaderLength;
    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = socketConns[0].s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;

    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    ipHeaderLength = ip->size * 4;

    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tcp->sequenceNumber = htonl(initialSeqNo);
    tcp->acknowledgementNumber = htonl(socketConns[0].s.sequenceNumber +
                                        socketConns[0].tcpSegLen);
    tcp->windowSize = tcpCurrWindow;
    tcp->urgentPointer = 0;
    tcp->offsetFields = 0;
    SETBIT(tcp->offsetFields,FIN);
    SETBIT(tcp->offsetFields,ACK);

    tcp->offsetFields &= ~(0xF000);
    tcpHdrlen = ((sizeof(tcpHeader)/4) << OFS_SHIFT) ; /* always it's *4 */
    tcp->offsetFields |= tcpHdrlen;

    tcp->offsetFields = htons(tcp->offsetFields);

    tcpLength = sizeof(tcpHeader) + 0; /*ack has no data */

    ip->length = htons(sizeof(ipHeader) + tcpLength) ;
    calcIpChecksum(ip);

    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tmp16 = htons(tcpLength);
    sumIpWords(&tmp16, 2, &sum);
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

/**
 * @brief tcpCreateSocket
 * 
 * @param remotePort 
 */
void tcpCreateSocket(uint16_t remotePort)
{
    socketConns[0].s.localPort     = LOCAL_PORT;
    socketConns[0].s.remotePort    = remotePort;

    getIpMqttBrokerAddress(socketConns[0].s.remoteIpAddress);

    ipEventIcb = IP_ARP_REQ_RESP;   
}

/**
 * @brief tcpHandleRwTransactions
 * 
 * @param ether 
 * @param flag 
 */
void tcpHandleRwTransactions(etherHeader *ether, uint8_t flag)
{
    //static uint8_t tcpWriteFSM = 0;
    static bool tcpReadFlag = 0;

    if(flag == TCP_TX)
    {
        switch(tcpWriteFSM)
        {
            case 0:
            {
                if(mqttGetTxStatus()) {
                    tcpWriteFSM = 1;
                    tcpReadFlag = false;
                }
            }
            break;

            case 1:
            {
                    uint8_t data[MQTT_SIZE] = {0};
                    uint16_t size;
                    /* maintain window pointers to send next packet or retransmit the packet */

                    mqttGetTxData(data, &size);
                    tcpSendSegment(ether, data, size,PSH,0);
                    tcpWriteFSM = 0;    /*since only 2 bytes we can set it here else in 
                                            tcpSendSegment */
                    /*start the timer*/
                    mqttSetTxStatus(false);
            }break;

            case 2:
            {
                /*wait state*/
                if(tcpReadFlag == true)
                {
                    mqttSetTxStatus(false);
                    if(true) { /* if more data needs to be sent go to state 1 */
                        tcpWriteFSM = 0; /*only 2 bytes it doesn't matter */
                    } else {
                        tcpWriteFSM = 1;
                    }
                }
                else
                {
                    /*timer if expired retransmit the packet if ack is not received */
                }

                /*have more no of bytes to send go back to zero and update the pointer
                else go back in either case*/
                if (true)
                {
                    
                }

            }break;

            default : break;
        }
        
    }
    
    if ( flag == TCP_RX)
    {   
        tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);
        
        socketConns[0].s.acknowledgementNumber = htonl(tcp->acknowledgementNumber);
        socketConns[0].s.sequenceNumber = htonl(tcp->sequenceNumber);
        socketConns[0].tcpSegLen = getTcpSegmentLength(ether);

        if(tcpValidChecks(ether) && 
            tcpIsAck(ether)
        ) 
        {
            //if(tcp->acknowledgementNumber == socketConns[0].s.sequenceNumber + 1) {
            if(isMqtt(ether)) {
                tcpReadFlag = true;
                /* collect the data for application */
                mqttSetRxData(ether);
                tcpSendAck(ether,0);
            } else {
                /*discard the packet*/
            }
        }
        else
        {
            /*wait*/
        }
    }
}

/**
 * @brief tcpSendSegment
 * 
 * @param ether 
 * @param data 
 * @param size 
 * @param flags 
 * @param ackVal 
 */
void tcpSendSegment(etherHeader *ether, uint8_t *data, uint16_t size, uint16_t flags,
                    uint8_t ackVal)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint16_t tcpHdrlen;
    uint8_t *ptr;
    uint8_t ipHeaderLength;
    /*uint8_t optionsField[] = {0x1,0x1,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,
                                0x9,0x10};*/

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = socketConns[0].s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    ipHeaderLength = ip->size * 4;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tcp->sequenceNumber = htonl(initialSeqNo);
    tcp->acknowledgementNumber =  htonl(socketConns[0].s.sequenceNumber +
                                        socketConns[0].tcpSegLen + ackVal);

    tcp->windowSize = tcpCurrWindow; //htons(MSS);
    tcp->urgentPointer = 0;
    tcp->offsetFields = 0;
    SETBIT(tcp->offsetFields,ACK);
    SETBIT(tcp->offsetFields,flags);

    tcpLength = sizeof(tcpHeader) +  /*sizeof(optionsField) +*/ size;
    //tcp->acknowledgementNumber =  htonl(htonl(tcp->sequenceNumber) + tcpLength);

    tcp->offsetFields &= ~(0xF000);
    tcpHdrlen = ((sizeof(tcpHeader)/4) << OFS_SHIFT) ; /* always it's *4 */
    tcp->offsetFields |= tcpHdrlen;

    tcp->offsetFields = htons(tcp->offsetFields);

    /*
    ptr = (uint8_t *)tcp->data;
    for(i = 0; i < sizeof(optionsField); i++) {
        ptr[i] = optionsField[i];
    }

    ptr += (uint8_t)i;
    */

    ptr = (uint8_t*)tcp->data;
    for(i = 0; i < size; i++) {
        ptr[i] = data[i];
    }

    ip->length = htons(sizeof(ipHeader) + tcpLength) ;
    calcIpChecksum(ip);

    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tmp16 = htons(tcpLength);
    sumIpWords(&tmp16, 2, &sum);
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);

    initialSeqNo += (size);
}

/**
 * @brief tcpValidChecks
 * 
 * @param ether 
 * @return true 
 * @return false 
 */
bool tcpValidChecks(etherHeader *ether)
{
    bool flag = false;
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);
    
    if(htons(tcp->destPort) == socketConns[0].s.localPort) {
        flag = true;
    } else {
        return false;
    }

    /* add more conditions if needed */
    return flag;
}

/**
 * @brief tcpCheckOptionsField
 * 
 * @param ether 
 */
void tcpCheckOptionsField(etherHeader *ether)
{
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);
    uint8_t size = tcpHeaderSize(ether);
    uint8_t i;
    uint8_t *ptr;
    uint8_t optionsSize;

    ptr = tcp->data;

    if(size > 5) {
        optionsSize = (size - 5)*4;
        socketConns[0].o.optFldFlag = true;
        for(i=0; i < optionsSize  && i < TCP_MAX_OPTIONS ; i++) {
            socketConns[0].o.optFldData[i] = ptr[i];
        }
        socketConns[0].o.optFldLen = i;
    }
}

/**
 * @brief displayStatus
 * 
 */
void displayStatus(void)
{
    uint8_t ip[4];
    char str[30];
    uint8_t i;

    getIpAddress(ip);
    putsUart0("  IP:                ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');

    putsUart0("  MQTT:              ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%"PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH-1)
            putcUart0('.');
    }
    putcUart0('\n');

    putsUart0("  MQTT CONN STATE:   ");
    snprintf(str, sizeof(str), "%s",mqttGetConnState() ? "MQTT_CONNECTED":"MQTT_DISCONNECTED");
    putsUart0(str);
    putcUart0('\n');

    putsUart0("  MQTT FSM STATE:    ");
    snprintf(str, sizeof(str), "%s",mqttFsmState[mqttGetCurrState()]);
    putsUart0(str);
    putcUart0('\n');

    putsUart0("  TCP STATE:         ");
    snprintf(str, sizeof(str), "%s",tcpState[getTcpCurrState(0)]);
    putsUart0(str);
    putcUart0('\n');

    putsUart0("  TCP WRITE STATE:         ");
    snprintf(str, sizeof(str), "%d",tcpWriteFSM);
    putsUart0(str);
    putcUart0('\n');
    

}
