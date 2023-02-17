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
#include "tcp.h"
#include "timer.h"
#include "mqtt.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
Tcb_t socketConns[NO_OF_SOCKETS] = {0};
//socket tcpSocket = {0};
static uint32_t initialAckNo = 0;
static uint32_t initialSeqNo = 0;
static bool initiateFin = false;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Determines whether packet is TCP packet
// Must be an IP packet
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
uint8_t *getTcpData(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    return tcp->data;
}

uint8_t *getTcpHeader(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    return (uint8_t*)tcp;
}

bool tcpIsSyn(etherHeader *ether)
{
    tcpHeader *tcpHdr = (tcpHeader*)getTcpHeader(ether);
    return ((tcpHdr->offsetFields & 0x0002) >> 1);

}

bool tcpIsAck(etherHeader *ether)
{
    tcpHeader *tcpHdr = (tcpHeader*)getTcpHeader(ether);
    return ((tcpHdr->offsetFields & 0x0010) >> 4);

}

/* header lenght is 4 bits in offset field */
uint16_t tcpHeaderSize(etherHeader *ether)
{
    tcpHeader *tcpHdr = (tcpHeader*)getTcpHeader(ether);
    return (tcpHdr->offsetFields >> (OFS_SHIFT - 1));
}

/***************************************************RX*******************************/
#if 0
void tcpFsmStateMachineServer(etherHeader *ether,uint8_t state)
{
    switch(state)
    {
        case TCP_CLOSED:
        {
            
        }break;

        case TCP_LISTEN:
        {

        }break;

        case TCP_SYN_RECEIVED:
        {

        }break;

        case TCP_SYN_SENT:
        {

            
        }break;

        case TCP_ESTABLISHED:
        {

        }break;

        case TCP_FIN_WAIT_1:
        {

        }break;

        case TCP_FIN_WAIT_2:
        {

        }break;

        case TCP_CLOSING:
        {

        }break;

        case TCP_CLOSE_WAIT:
        {

        }break;

        case TCP_LAST_ACK:
        {

        }break;

        case TCP_TIME_WAIT:
        {

        }break;

        default: break;
    }
}
#endif

uint32_t genRandNum(void)
{
    srand(rand());

    return (rand()*10*3);
}
void tcpHandlerRx(etherHeader *ether)
{
    uint8_t socketNo = 0;

    /* check which socket's ip it belongs */
    if( (socketNo = tcpGetMatchingSocket(ether)) != TCP_INV_VAL ) {
        tcpFsmStateMachineClient(ether, socketNo, TCP_RX);
    }

}

uint8_t tcpGetMatchingSocket(etherHeader *ether)
{
    uint8_t i;
    uint8_t j;
    ipHeader* ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)getTcpHeader(ether);
    uint8_t localIp[4];

    getIpAddress(localIp);

    for(i = 0; i < NO_OF_SOCKETS; i++) {

        if( (socketConns[i].s.localPort == tcp->destPort)
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
void tcpHandlerTx(etherHeader *ether)
{
    uint8_t count = 0;

    /* handling the tx data */
    for(count = 0; count < NO_OF_SOCKETS; count++) {
        socketConns[count].socketIdx = count;
        tcpFsmStateMachineClient(ether,count, TCP_TX);
    }

}

void tcpFsmStateMachineClient(etherHeader *ether, uint8_t Idx, uint8_t flag)
{
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);

    switch(socketConns[Idx].fsmState)
    {
        case TCP_CLOSED:
        {
            if(true == socketConns[Idx].initConnect) {

                if(true) {
                    tcpSendSyn(ether);
                    socketConns[Idx].fsmState = TCP_SYN_SENT;
                } else {
                    /*Extra work: get remote hw mac using arp */
                }
                
            }
            
        }break;

        case TCP_SYN_SENT:
        {
            /* awaits syn+ack from servers and sends an ack and enters established */

            if( (tcp->offsetFields & ACK ) && 
                (tcp->offsetFields & SYN)
            )
            {  
                /*send ack */
                tcpSendAck(ether);
                socketConns[0].s.acknowledgementNumber = tcp->acknowledgementNumber;
                socketConns[Idx].fsmState = TCP_ESTABLISHED;

            } else {
                socketConns[Idx].fsmState = TCP_CLOSED;
            }
            
        }break;

        case TCP_ESTABLISHED:
        {
            /* received fin ? */
            if(tcp->offsetFields & FIN)
            {
                tcpSendAck(ether);
                socketConns[Idx].fsmState = TCP_CLOSE_WAIT;
            }
            else if (initiateFin)
            {
                tcpSendFin(ether);
                socketConns[Idx].fsmState = TCP_FIN_WAIT_1;
            }
            else {
                tcpHandleRwTransactions(ether, flag);
            }

        }break;

        case TCP_FIN_WAIT_1:
        {
            if(tcp->offsetFields & ACK)
            {
                socketConns[Idx].fsmState = TCP_FIN_WAIT_2;
            }

        }break;

        case TCP_FIN_WAIT_2:
        {
            if(tcp->offsetFields & FIN)
            {
                tcpSendAck(ether);
                socketConns[Idx].fsmState = TCP_TIME_WAIT;
            }
        }break;

        case TCP_CLOSING:
        {

        }break;

        case TCP_CLOSE_WAIT:
        {
            tcpSendFin(ether);
            socketConns[Idx].fsmState = TCP_LAST_ACK;

        }break;

        case TCP_LAST_ACK:
        {
            if(tcp->offsetFields & ACK)
            {
                socketConns[Idx].fsmState = TCP_CLOSED;
            }

        }break;

        case TCP_TIME_WAIT:
        {
            socketConns[Idx].fsmState = TCP_CLOSED;
        }break;

        case TCP_LISTEN:
        case TCP_SYN_RECEIVED:
            /* for server */
        default: break;
    }
}

/* function needs to be set to initate tx */
void tcpConnect(uint8_t socketno)
{
    /* set the socket connection to syn sent */
    socketConns[socketno].initConnect = true;
}

uint8_t getTcpCurrState(uint8_t i)
{
    return socketConns[i].fsmState;
}

#if 0
void tcpMakeSegment(etherHeader *ether, socket s)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t udpLength;
    uint8_t *copyData;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = tcpSocket.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = tcpSocket.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
}
#endif

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

void tcpSendSyn(etherHeader *ether)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint32_t tmp32 = 0;
    uint16_t tcpHdrlen = 0;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint8_t *ptr = NULL;
    uint8_t optionsField[] = {0x2, 0x4, 0x5, 0xB4};

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
    uint8_t ipHeaderLength = ip->size * 4;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tmp32 = genRandNum();
    tcp->sequenceNumber = initialSeqNo = socketConns[0].s.sequenceNumber = tmp32; /*TODO needed htonl ?*/
    tcp->acknowledgementNumber = 0;
    tcp->windowSize = htons(MSS);
    tcp->urgentPointer = 0;
    tcp->offsetFields = 0;
    SETBIT(tcp->offsetFields,SYN);
    
    tcpLength = sizeof(tcpHeader) + sizeof(optionsField)+ 0; /*syn has no data */
    
    tcp->offsetFields &= ~(0xF000);
    tcpHdrlen = ((sizeof(tcpHeader)/4) << OFS_SHIFT) ; /* always it's *4 */
    tcp->offsetFields |= tcpHdrlen;

    tcp->offsetFields = htons(tcp->offsetFields);

    
    ptr = (uint8_t *)tcp->data[0];
    for(i = 0; i < sizeof(optionsField); i++) {
        ptr[i] = optionsField[i];
    }
    

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

void tcpSendAck(etherHeader *ether)
{
    //socket s;
    //getTcpMessageSocket(ether,&s);
    //tcpMakeSegment(ether,s);

    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

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
    uint8_t ipHeaderLength = ip->size * 4;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tcp->sequenceNumber = socketConns[0].s.sequenceNumber;
    tcp->acknowledgementNumber = 0;
    tcp->windowSize = MSS;
    tcp->urgentPointer = 0;
    SETBIT(tcp->offsetFields,ACK);
    calcIpChecksum(ip);
    tcpLength = sizeof(tcpHeader) + 0; /*ack has no data */

    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sumIpWords(&tcpLength, 2, &sum); /* doubt in this */
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

void tcpSendFin(etherHeader *ether)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

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
    uint8_t ipHeaderLength = ip->size * 4;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tcp->sequenceNumber = socketConns[0].s.sequenceNumber;
    tcp->acknowledgementNumber = 0;
    tcp->windowSize = MSS;
    tcp->urgentPointer = 0;
    SETBIT(tcp->offsetFields,FIN);
    calcIpChecksum(ip);
    tcpLength = sizeof(tcpHeader) + 0; /*ack has no data */

    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sumIpWords(&tcpLength, 2, &sum); /* doubt in this */
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

void tcpCreateSocket(uint16_t remotePort, uint8_t *toIp)
{
    uint8_t i;
    uint8_t remoteHwMac[] = {12,3,4,5,6,9}; /* need to get response from server in arp */

    /* TODO need to come up with generic socket no instead of 0 */
    socketConns[0].s.localPort     = LOCAL_PORT;
    socketConns[0].s.remotePort    = remotePort;

    getIpMqttBrokerAddress(socketConns[0].s.remoteIpAddress);

    for (i = 0; i < HW_ADD_LENGTH; i++)
        socketConns[0].s.remoteHwAddress[i] = remoteHwMac[i];
    
}

void tcpHandleRwTransactions(etherHeader *ether, uint8_t flag)
{
    if(flag == TCP_TX)
    {
        if(mqttGetTxStatus()) {

            uint8_t data[MQTT_PAYLOAD_SIZE] = {0};
            uint16_t size;

            mqttPubGetData(data, &size);
            tcpSendSegment(ether, data, size);
            mqttSetTxStatus(false);/*since only 2 bytes we can set it here else in 
                                    tcpSendSegment */
        }
    }
    
    if ( flag == TCP_RX)
    {   
        
    }
}

void tcpSendSegment(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];
    uint8_t *ptr = NULL;

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
    uint8_t ipHeaderLength = ip->size * 4;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
     
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = socketConns[0].s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // Tcp header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(socketConns[0].s.localPort);
    tcp->destPort = htons(socketConns[0].s.remotePort);
    tcp->sequenceNumber = socketConns[0].s.sequenceNumber;
    tcp->acknowledgementNumber = socketConns[0].s.acknowledgementNumber;
    tcp->windowSize = MSS;
    tcp->urgentPointer = 0;
    SETBIT(tcp->offsetFields,FIN);
    calcIpChecksum(ip);
    tcpLength = sizeof(tcpHeader) + size; 

    ptr = tcp->data;
    for(i = 0; i < size; i++) {
        ptr[i] = data[i];
    }

    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sumIpWords(&tcpLength, 2, &sum); /* doubt in this */
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}
