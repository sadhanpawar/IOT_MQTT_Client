// MQTT Library (framework only)
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
#include <inttypes.h>
#include "mqtt.h"
#include "timer.h"
#include "tcp.h"
#include "uart0.h"
#include "tcp.h"

uint16_t mqttPktId = 0x1234;
// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
static mqttMcb_t mqttMcb = {0};
static mqttRxBf_t mqttRxBuffer = {0};
static bool initiateConnectReq = false;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
void mqttPublish(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint8_t i;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_PUBLISH;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_ASS_DELIVERY;
    mqtt->retain = 0;

    /*variable header*/

    /*length*/
    varHdrPtr = mqtt->data;
    *varHdrPtr = (mqttMcb.topicSize & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttMcb.topicSize & 0x00FF);
    varLen += 2;

    varHdrPtr += (uint16_t)1;/*??*/

    /*topic name*/
    for(i = 0 ; i < mqttMcb.topicSize; i ++) {
        varHdrPtr[i] = data[i];
    }
    varLen += mqttMcb.topicSize;

    *(varHdrPtr + i)= (mqttPktId & 0xFF00) >> 8;
    *(varHdrPtr+ i + 1) = (uint8_t)(mqttPktId & 0x00FF);

    varLen += 2;

    /*data*/
    for(i = mqttMcb.topicSize; i < mqttMcb.topicSize + (mqttMcb.totalSize - mqttMcb.topicSize) ; i ++) {
        varHdrPtr[i] = data[i];
    }

    varLen += (mqttMcb.totalSize - mqttMcb.topicSize);
    
    mqtt->remLength = varLen;

    mqttSetTxStatus(true);
}

void mqttSubscribe(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint8_t i;
    uint16_t varLen = 0;
    uint8_t reqQos = 1;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_SUBSCRIBE;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = 1;
    mqtt->retain = 0;

    /*variable header*/

    varHdrPtr = mqtt->data;

    /*packet ID*/
    *(varHdrPtr + 0)= (mqttPktId & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttPktId & 0x00FF);

    varLen += 2;

    varHdrPtr += 2;

    /*topic length*/
    *varHdrPtr = (mqttMcb.topicSize & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttMcb.topicSize & 0x00FF);
    varLen += 2;

    varHdrPtr += 2;

    /*topic name*/
    for(i = 0 ; i < mqttMcb.topicSize; i ++) {
        varHdrPtr[i] = data[i];
    }
    varLen += mqttMcb.topicSize;
    varHdrPtr += (uint8_t)mqttMcb.topicSize;

    /*requested qos*/
    *varHdrPtr = reqQos;
    varLen += 1;
    
    mqtt->remLength = varLen;

    mqttSetTxStatus(true);
}

void mqttUnSubscribe(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint8_t i;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_UNSUBSCRIBE;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = 1;
    mqtt->retain = 0;

    /*variable header*/

    varHdrPtr = mqtt->data;

    /*packet ID*/
    *(varHdrPtr + 0)= (mqttPktId & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttPktId & 0x00FF);

    varLen += 2;

    varHdrPtr += 2;

    /*topic length*/
    *varHdrPtr = (mqttMcb.topicSize & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttMcb.topicSize & 0x00FF);
    varLen += 2;

    varHdrPtr += 2;

    /*topic name*/
    for(i = 0 ; i < mqttMcb.topicSize; i ++) {
        varHdrPtr[i] = data[i];
    }
    varLen += mqttMcb.topicSize;
    varHdrPtr += (uint8_t)mqttMcb.topicSize;
    
    mqtt->remLength = varLen;

    mqttSetTxStatus(true);
}

void mqttConnect(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint16_t varLen = 0;
    uint8_t i;

    #if 0
    mqttMcb.data[0] = 0x10;
    mqttMcb.data[1] = 0x25;
    mqttMcb.data[2] = 0x0;
    mqttMcb.data[3] = 0x6;
    mqttMcb.data[4] = 0x4d;
    mqttMcb.data[5] = 0x51;
    mqttMcb.data[6] = 0x49;
    mqttMcb.data[7] = 0x73;
    mqttMcb.data[8] = 0x64;
    mqttMcb.data[9] = 0x70;
    mqttMcb.data[10] = 0x03;
    mqttMcb.data[11] = 0x02;
    mqttMcb.data[12] = 0x00;
    mqttMcb.data[13] = 0x05;
    mqttMcb.data[14] = 0x00;
    mqttMcb.data[15] = 0x17;
    mqttMcb.data[16] = 0x70;
    mqttMcb.data[17] = 0x61;
    mqttMcb.data[18] = 0x68;
    mqttMcb.data[19] = 0x6f;
    mqttMcb.data[20] = 0x2f;
    mqttMcb.data[21] = 0x33;
    mqttMcb.data[22] = 0x34;
    mqttMcb.data[23] = 0x41;
    mqttMcb.data[24] = 0x41;
    mqttMcb.data[25] = 0x45;
    mqttMcb.data[26] = 0x35;
    mqttMcb.data[27] = 0x34;
    mqttMcb.data[28] = 0x41;
    mqttMcb.data[29] = 0x37;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.data[30] = 0x35;
    mqttMcb.totalSize = MQTT_SIZE;
    #endif

    #if 1
    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_CONNECT;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = 0;
    mqtt->retain = 0;

    /*variable header*/
    varHdrPtr = mqtt->data;

    /*protocol name*/
    *(varHdrPtr + 0)    = 0;
    *(varHdrPtr + 1)    = 4;
    *(varHdrPtr + 2)    = 'M';
    *(varHdrPtr + 3)    = 'Q';
    *(varHdrPtr + 4)    = 'T';
    *(varHdrPtr + 5)    = 'T';
    varLen += 6;

    /*protocol level*/
    *(varHdrPtr + 6)    = 4;
    varLen += 1;

    /* connect flag */
    *(varHdrPtr + 7)    = 0x2;
    varLen += 1;

    /*keep alive*/
    *(varHdrPtr + 8)    = 0x0;
    *(varHdrPtr + 9)    = 0x5; /*10 secs*/
    varLen += 2;

    /*client ID length*/
    *(varHdrPtr + 10)    = 0x0;
    *(varHdrPtr + 11)    = 0x17;
    varLen += 2;

    /*client ID */
    for(i = 12; i < 35; i++) {
        *(varHdrPtr + i)    = i - 12;
    }
    varLen += (i-12);
    
    mqtt->remLength = varLen; /*var header + payload lenght */
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;
    #endif

    mqttSetTxStatus(true);
}


void mqttDisConnect(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_DISCONNECT;
    mqtt->dup = 0;
    mqtt->qosLevel = 0;
    mqtt->retain = 0;

    /* No variable header*/

    mqtt->remLength = varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/*TODO : might have to implement a state machine to split the larger data packets 
to feed into tcp/ip if it exceeds MTU
*/

void mqttLogPublishEvent(char *data, uint16_t size, bool flag)
{
    uint8_t i = 0;
    uint8_t j = 0;
    mqttMcb.mqttEvent = PUBLISH;
    initiateConnectReq = true;
    
    if(flag == 0) {
        for(i = 0; i < size; i++) {
            mqttMcb.args[i] = data[i];
        }
        mqttMcb.topicSize = size;
        mqttMcb.totalSize = size;
    } else {
        for(i = mqttMcb.topicSize,j=0; i < (mqttMcb.topicSize+size); i++,j++) {
            mqttMcb.args[i] = data[j];
        }
        mqttMcb.dataSize = size;
        mqttMcb.totalSize += size;
    }
}

void mqttLogSubscribeEvent(char *data, uint16_t size)
{
    uint8_t i = 0;
    mqttMcb.mqttEvent = SUBSCRIBE;
    initiateConnectReq = true;
    
    for(i = 0; i < size; i++) {
        mqttMcb.args[i] = data[i];
    }
    mqttMcb.topicSize = size;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = size;
}

void mqttLogUnSubscribeEvent(char *data, uint16_t size)
{
    uint8_t i = 0;
    mqttMcb.mqttEvent = UNSUBSCRIBE;
    initiateConnectReq = true;
    
    for(i = 0; i < size; i++) {
        mqttMcb.args[i] = data[i];
    }
    mqttMcb.topicSize = size;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = size;
}

void mqttLogConnectEvent(void)
{
    mqttMcb.mqttEvent = CONNECT;
    initiateConnectReq = true;

    mqttMcb.topicSize = 0;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = 0;
}

void mqttLogDisConnectEvent(void)
{
    mqttMcb.mqttEvent = DISCONNECT;
    initiateConnectReq = true;

    mqttMcb.topicSize = 0;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = 0;
}

void mqttHandler(etherHeader *ether )
{
    char str[80];

    switch (mqttMcb.mqttEvent)
    {
        case PUBLISH:
        {
            /*send the mqtt request after it gets connected */
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the the topic and data with mqtt message */
                    mqttPublish(ether, mqttMcb.data, mqttMcb.totalSize);
                    mqttMcb.mqttEvent = PUBLISH_WAIT;
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateConnectReq) {
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateConnectReq = false;
            }
            
        }break;

        case PUBLISH_WAIT:
        {
            /*wait for pub ack */
            if(mqttRxBuffer.receviedData) {
                mqttRxBuffer.receviedData = false;
                mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;
                uint8_t *varHdrPtr;

                if( (mqtt->controlPacket == MQ_PUBACK) 
                )
                {
                    varHdrPtr = mqtt->data;

                    if(varHdrPtr[0] == ((mqttPktId & 0xFF00) >>8) &&
                        varHdrPtr[1] == ((uint8_t)(mqttPktId & 0x00FF))
                    )
                    {
                        snprintf(str, sizeof(str), "PUBLISH: success with Pkt ID:""%"PRIu16, mqttPktId);
                        putsUart0(str);
                        mqttMcb.mqttEvent = NOEVENT;
                    }
                }
            } else {
                /*wait*/
            }

        }break;

        case SUBSCRIBE:
        {
            /*send the mqtt request after it gets connected */
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the the topic and data with mqtt message */
                    mqttSubscribe(ether, mqttMcb.data, mqttMcb.totalSize);
                    mqttMcb.mqttEvent = SUBSCRIBE_WAIT;
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateConnectReq = false;
            }

        }break;

        case SUBSCRIBE_WAIT:
        {
            /*wait for sub ack */
            if(mqttRxBuffer.receviedData) {
                mqttRxBuffer.receviedData = false;
                mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;
                uint8_t *varHdrPtr;

                if( (mqtt->controlPacket == MQ_SUBACK) 
                )
                {
                    varHdrPtr = mqtt->data;

                    if(varHdrPtr[0] == ((mqttPktId & 0xFF00) >>8) &&
                        varHdrPtr[1] == ((uint8_t)(mqttPktId & 0x00FF))
                    )
                    {
                        snprintf(str, sizeof(str), "SUBSCRIBE: success with Pkt ID:""%"PRIu16, mqttPktId);
                        putsUart0(str);
                        mqttMcb.mqttEvent = NOEVENT;
                    }
                }
            } else {
                /*wait*/
            }

        }break;

        case UNSUBSCRIBE:
        {
            /*send the mqtt request after it gets connected */
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the the topic and data with mqtt message */
                    mqttUnSubscribe(ether, mqttMcb.data, mqttMcb.totalSize);
                    mqttMcb.mqttEvent = NOEVENT;
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateConnectReq = false;
            }

        }break;

        case UNSUBSCRIBE_WAIT:
        {
            /*wait  */
            /*wait for sub ack */
            if(mqttRxBuffer.receviedData) {
                mqttRxBuffer.receviedData = false;
                mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;
                uint8_t *varHdrPtr;

                if( (mqtt->controlPacket == MQ_UNSUBACK) 
                )
                {
                    varHdrPtr = mqtt->data;

                    if(varHdrPtr[0] == ((mqttPktId & 0xFF00) >>8) &&
                        varHdrPtr[1] == ((uint8_t)(mqttPktId & 0x00FF))
                    )
                    {
                        snprintf(str, sizeof(str), "UNSUBSCRIBE: success with Pkt ID:""%"PRIu16, mqttPktId);
                        putsUart0(str);
                        mqttMcb.mqttEvent = NOEVENT;
                    }
                }
            } else {
                /*wait*/
            }

        }break;

        case CONNECT:
        {
            /*send the mqtt request after it gets connected */
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the the topic and data with mqtt message */
                    mqttConnect(ether, mqttMcb.data, mqttMcb.totalSize);
                    mqttMcb.mqttEvent = CONNECT_WAIT;
                    /*TODO start the timer*/
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateConnectReq = false;
            }

        }break;

        case CONNECT_WAIT:
        {
            /*wait  */
            /*wait for sub ack */
            if(mqttRxBuffer.receviedData) {
                mqttRxBuffer.receviedData = false;
                mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;
                uint8_t *varHdrPtr;

                if( (mqtt->controlPacket == MQ_CONNACK) 
                )
                {
                    varHdrPtr = mqtt->data;

                    if(*(varHdrPtr+1) == 0) /*byte 2*/
                    {
                        snprintf(str, sizeof(str), "CONNECT:connected successfully with MQTT broker\n");
                    }
                    else if(*(varHdrPtr+1) == 1) 
                    {
                        snprintf(str, sizeof(str), "CONNECT:connected refused with protocol version\n");
                    }
                    else if(*(varHdrPtr+1) == 2) 
                    {
                        snprintf(str, sizeof(str), "CONNECT:connected refused identifier rejected\n");
                    }
                    else
                    {
                        snprintf(str, sizeof(str), "CONNECT:unknown code returned\n");
                    }
                    putsUart0(str);
                    mqttMcb.mqttEvent = NOEVENT;
                }
            } else {
                /*wait*/ //timer
            }

        }break;

        case DISCONNECT:
        {
            /*send the mqtt request after it gets connected */
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the the topic and data with mqtt message */
                    mqttDisConnect(ether, mqttMcb.data, mqttMcb.totalSize);
                    snprintf(str, sizeof(str), "DISCONNECT:disconnected successfully with MQTT broker\n");
                    putsUart0(str);
                    mqttMcb.mqttEvent = NOEVENT;
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */
                tcpConnect(0);
                initiateConnectReq = false;
            }

        }break;

        case DISCONNECT_WAIT:
        {
            /*wait  */

        }break;


        case NOEVENT:
        default: break;
    }
}

bool mqttGetTxStatus(void)
{
    return mqttMcb.initiateTrans;
}

void mqttSetTxStatus(bool status)
{
    mqttMcb.initiateTrans = status;
}

void mqttGetTxData(uint8_t *data, uint16_t *size)
{
    uint8_t i;

    for(i = 0; i < mqttMcb.totalSize; i++)
    {
        data[i] = mqttMcb.data[i];
    }
    *size = mqttMcb.totalSize;
}

bool isMqtt(etherHeader *ether)
{
    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);
    mqttHeader *mqtt = (mqttHeader*)tcp->data;

    uint8_t flag = mqtt->controlPacket;

    switch(flag)
    {
        case MQ_CONNECT     :
        case MQ_CONNACK     :  
        case MQ_PUBLISH     :  
        case MQ_PUBACK      :  
        case MQ_PUBREC      :  
        case MQ_PUBREL      :  
        case MQ_PUBCOMP     :  
        case MQ_SUBSCRIBE   :  
        case MQ_SUBACK      :  
        case MQ_UNSUBSCRIBE :  
        case MQ_UNSUBACK    :  
        case MQ_PINGREQ     :  
        case MQ_PINGRESP    :  
        case MQ_DISCONNECT  :  
        case MQ_AUTH        :  
            return true;
        default : break;
    }
    return false;
}

void mqttSetRxData(etherHeader *ether)
{
    uint8_t i;

    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);

    uint8_t *mqtt = (uint8_t*)tcp->data;
    uint8_t remlength = mqtt[1];

    for(i = 0; i < remlength; i++)
    {
        mqttRxBuffer.data[i] = mqtt[i];
    }
    mqttRxBuffer.receviedData = true;
}

void mqttInit(void)
{
    /* create a socket and bind it to initiate socket communication 
        when triggered
    */
    tcpCreateSocket(MQTT_PORT);
}
