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

uint16_t mqttPktId = 0x0001;
// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
static mqttMcb_t mqttMcb = {0};
static mqttRxBf_t mqttRxBuffer = {0};
static bool initiateTcpConnectReq = false;
static uint8_t mqttConnStatus = MQTT_DISCONNECTED;
static bool mqttServerConnectStatus = false;
static uint16_t keepAliveSrvTimer = 0;
static bool mqttConTimerFlag = false;
static char mqttCurSubTopics[MQTT_NO_SUB_TOPICS][MQTT_TOPIC_LEN] = {0};
static uint8_t topicIdx = 0;
static uint16_t keepAliveCliTimer = 0;

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------
char *mqttState[] = {

        "MQ_CONNECT     ",
        "MQ_CONNACK     ",  
        "MQ_PUBLISH     ",  
        "MQ_PUBACK      ",  
        "MQ_PUBREC      ",  
        "MQ_PUBREL      ",  
        "MQ_PUBCOMP     ",  
        "MQ_SUBSCRIBE   ",  
        "MQ_SUBACK      ",  
        "MQ_UNSUBSCRIBE ",  
        "MQ_UNSUBACK    ",  
        "MQ_PINGREQ     ",  
        "MQ_PINGRESP    ",  
        "MQ_DISCONNECT  ",  
        "MQ_AUTH        " 
};

char *mqttFsmState[] = {
    "NOEVENT",
    "PUBLISH",
    "PUBLISH_WAIT",
    "SUBSCRIBE",
    "SUBSCRIBE_WAIT",
    "UNSUBSCRIBE",
    "UNSUBSCRIBE_WAIT",
    "CONNECT",
    "CONNECT_WAIT",
    "DISCONNECT",
    "DISCONNECT_WAIT",
    "PING_REQUEST",
    "PING_RESPONSE"
};
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

/**
 * @brief mqttPublish
 * 
 * @param ether 
 * @param data 
 * @param size 
 */
void mqttPublish(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint8_t i,j;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_PUBLISH;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_ACK_DELIVERY;
    mqtt->retain = 1;

    /*variable header*/

    /*topic length*/
    varHdrPtr = mqtt->data;
    *varHdrPtr = (mqttMcb.topicSize & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttMcb.topicSize & 0x00FF);
    varLen += 2;

    varHdrPtr += (uint8_t)2;

    /*topic name*/
    for(i = 0 ; i < mqttMcb.topicSize; i ++) {
        varHdrPtr[i] = mqttMcb.args[i];
    }
    varLen += mqttMcb.topicSize;
    varHdrPtr += i;

    /*Packet ID*/
    *(varHdrPtr + 0)= (mqttPktId & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttPktId & 0x00FF);

    varHdrPtr += 2;
    varLen += 2;

    /*data*/
    for(i = mqttMcb.topicSize,j=0; i < mqttMcb.topicSize + (mqttMcb.totalSize - mqttMcb.topicSize) ; i++,j++) {
        varHdrPtr[j] = mqttMcb.args[i];
    }

    varLen += (mqttMcb.totalSize - mqttMcb.topicSize);
    
    /*message length*/
    mqtt->remLength =  varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/**
 * @brief mqttSubscribe
 * 
 * @param ether 
 * @param data 
 * @param size 
 */
void mqttSubscribe(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint8_t i;
    uint16_t varLen = 0;
    uint8_t reqQos = MQ_FIRE_FORGET;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_SUBSCRIBE;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_ACK_DELIVERY;
    mqtt->retain = 0;

    /*variable header*/

    varHdrPtr = mqtt->data;

    /*packet ID/Message Identifier*/
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
        varHdrPtr[i] = mqttMcb.args[i];
    }
    varLen += mqttMcb.topicSize;
    varHdrPtr += i;

    strncpy(mqttCurSubTopics[topicIdx],mqttMcb.args,mqttMcb.topicSize);

    /*requested qos*/
    *varHdrPtr = reqQos;
    varLen += 1;
    
    mqtt->remLength = varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/**
 * @brief mqttUnSubscribe
 * 
 * @param ether 
 * @param data 
 * @param size 
 */
void mqttUnSubscribe(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint8_t i;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_UNSUBSCRIBE;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_ACK_DELIVERY;
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
        varHdrPtr[i] = mqttMcb.args[i];
    }
    varLen += mqttMcb.topicSize;
    varHdrPtr += i;
    
    mqtt->remLength = varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/**
 * @brief mqttConnect
 * 
 * @param ether 
 * @param data 
 * @param size 
 */
void mqttConnect(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint8_t *varHdrPtr;
    uint16_t varLen = 0;
    uint8_t i;
    char clientId[] = "sadhan/0123456789ABCDEF";

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
    *(varHdrPtr + 9)    = 0x14; /*20 secs*/
    varLen += 2;

    keepAliveCliTimer = *(varHdrPtr + 8);
    keepAliveCliTimer = (keepAliveCliTimer << 8) | *(varHdrPtr + 9);
    startPeriodicTimer(mqttKeepAliveTimeCliCb,keepAliveCliTimer - 1);

    /*client ID length*/
    *(varHdrPtr + 10)    = 0x0;
    *(varHdrPtr + 11)    = 0x17;
    varLen += 2;

    /*client ID */
    varHdrPtr += 12;

    strncpy((char*)varHdrPtr,clientId,strlen(clientId));
    varLen += strlen(clientId);
    
    mqtt->remLength = varLen; /*var header + payload lenght */
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/**
 * @brief mqttDisConnect
 * 
 * @param ether 
 * @param data 
 * @param size 
 */
void mqttDisConnect(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_DISCONNECT;
    mqtt->dup = 0;
    mqtt->qosLevel = MQ_FIRE_FORGET;
    mqtt->retain = 0;

    /* No variable header*/

    mqtt->remLength = varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    initiateFin = true;
}

/**
 * @brief mqttPingRequest
 * 
 * @param ether 
 */
void mqttPingRequest(etherHeader *ether, uint8_t *data, uint16_t size)
{
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_PINGREQ;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_FIRE_FORGET;
    mqtt->retain = 0;
    
    /*message length*/
    mqtt->remLength =  varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/*TODO : might have to implement a state machine to split the larger data packets 
to feed into tcp/ip if it exceeds MTU
*/

/**
 * @brief mqttLogPublishEvent
 * 
 * @param data 
 * @param size 
 * @param flag 
 */
void mqttLogPublishEvent(char *data, uint16_t size, bool flag)
{
    uint8_t i = 0;
    uint8_t j = 0;
    mqttMcb.mqttEvent = PUBLISH;
    initiateTcpConnectReq = true;
    
    if(flag == 0) {
        for(i = 0; i < size; i++) {
            mqttMcb.args[i] = data[i];
        }
        mqttMcb.topicSize = size;
        mqttMcb.totalSize = size;
    } else {
        for(i = mqttMcb.topicSize,j=0; i < (mqttMcb.topicSize+size); i++,j++) {
            mqttMcb.args[i] = data[j]; /*since mqttMcb.data is being used to send actual data
                                        in packet*/
        }
        mqttMcb.dataSize = size;
        mqttMcb.totalSize += size;
    }
}

/**
 * @brief mqttLogSubscribeEvent
 * 
 * @param data 
 * @param size 
 */
void mqttLogSubscribeEvent(char *data, uint16_t size)
{
    uint8_t i = 0;
    mqttMcb.mqttEvent = SUBSCRIBE;
    initiateTcpConnectReq = true;
    
    for(i = 0; i < size; i++) {
        mqttMcb.args[i] = data[i];
    }
    mqttMcb.topicSize = size;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = size;
}

/**
 * @brief mqttLogUnSubscribeEvent
 * 
 * @param data 
 * @param size 
 */
void mqttLogUnSubscribeEvent(char *data, uint16_t size)
{
    uint8_t i = 0;
    mqttMcb.mqttEvent = UNSUBSCRIBE;
    initiateTcpConnectReq = true;
    
    for(i = 0; i < size; i++) {
        mqttMcb.args[i] = data[i];
    }
    mqttMcb.topicSize = size;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = size;
}

/**
 * @brief mqttLogConnectEvent
 * 
 */
void mqttLogConnectEvent(void)
{
    mqttMcb.mqttEvent = CONNECT;
    initiateTcpConnectReq = true;

    mqttMcb.topicSize = 0;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = 0;
}

/**
 * @brief mqttLogDisConnectEvent
 * 
 */
void mqttLogDisConnectEvent(void)
{
    mqttMcb.mqttEvent = DISCONNECT;
    initiateTcpConnectReq = true;

    mqttMcb.topicSize = 0;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = 0;
}

/**
 * @brief mqttHandler
 * 
 * @param ether 
 */
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
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateTcpConnectReq) {
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateTcpConnectReq = false;
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
                        snprintf(str, sizeof(str), "PUBLISH: success with Pkt ID:""%"PRIX16"\n", mqttPktId);
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
                    snprintf(str, sizeof(str), "SUBSCRIBE: Request with Pkt ID:""%"PRIX16"\n", mqttPktId);
                    putsUart0(str);
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateTcpConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateTcpConnectReq = false;
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
                        snprintf(str, sizeof(str), "SUBSCRIBE: success with Pkt ID:""%"PRIX16"\n", mqttPktId);
                        putsUart0(str);
                        mqttMcb.mqttEvent = NOEVENT;
                    }
                }
                else
                {
                    snprintf(str, sizeof(str), "SUBSCRIBE: No SUBACK received\n");
                    putsUart0(str);
                    mqttMcb.mqttEvent = NOEVENT;
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
                    mqttMcb.mqttEvent = UNSUBSCRIBE_WAIT;
                    snprintf(str, sizeof(str), "UNSUBSCRIBE: Request with Pkt ID:""%"PRIX16"\n", mqttPktId);
                    putsUart0(str);
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateTcpConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateTcpConnectReq = false;
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
                        snprintf(str, sizeof(str), "UNSUBSCRIBE: success with Pkt ID:""%"PRIX16"\n", mqttPktId);
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
                    /* start the timer*/
                    startPeriodicTimer(mqttConTimerCb,3);
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateTcpConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */ 
                tcpConnect(0);
                initiateTcpConnectReq = false;
            }

        }break;

        case CONNECT_WAIT:
        {
            /*wait  */
            if(mqttConTimerFlag) {
                mqttLogConnectEvent();
            }
            /*wait for sub ack */
            else if(mqttRxBuffer.receviedData) {
                mqttRxBuffer.receviedData = false;
                stopTimer(mqttConTimerCb);
                mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;
                uint8_t *varHdrPtr;

                if( (mqtt->controlPacket == MQ_CONNACK) 
                )
                {
                    varHdrPtr = mqtt->data;

                    if(*(varHdrPtr+1) == 0) /*byte 2*/
                    {
                        snprintf(str, sizeof(str), "CONNECT:connected successfully with MQTT broker\n");
                        mqttConnStatus = MQTT_CONNECTED;
                    }
                    else if(*(varHdrPtr+1) == 1) 
                    {
                        snprintf(str, sizeof(str), "CONNECT:connected refused with protocol version\n");
                        mqttConnStatus = MQTT_DISCONNECTED;
                    }
                    else if(*(varHdrPtr+1) == 2) 
                    {
                        snprintf(str, sizeof(str), "CONNECT:connected refused identifier rejected\n");
                        mqttConnStatus = MQTT_DISCONNECTED;
                    }
                    else
                    {
                        snprintf(str, sizeof(str), "CONNECT:unknown code returned\n");
                        mqttConnStatus = MQTT_DISCONNECTED;
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
                    mqttConnStatus = MQTT_DISCONNECTED;
                    stopTimer(mqttKeepAliveTimeSrvCb);
                    stopTimer(mqttKeepAliveTimeCliCb);
            } else if (getTcpCurrState(0) == TCP_CLOSED && initiateTcpConnectReq) {
               
                /* create a socket and bind it to initiate socket communication */
                tcpConnect(0);
                initiateTcpConnectReq = false;
            }

        }break;

        case DISCONNECT_WAIT:
        {
            /*wait  */

        }break;

        case PING_REQUEST:
        {
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the ping request mqtt message */
                    mqttPingRequest(ether, mqttMcb.data, mqttMcb.totalSize);
                    snprintf(str, sizeof(str), "PING_REQ: MQTT broker\n");
                    putsUart0(str);
                    mqttMcb.mqttEvent = PING_RESPONSE;
            } else {
                /*leave it */
            }

        }break;

        case PING_RESPONSE:
        {
            if(mqttRxBuffer.receviedData) {
                mqttRxBuffer.receviedData = false;
                mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;

                if( (mqtt->controlPacket == MQ_PINGRESP) 
                )
                {
                        snprintf(str, sizeof(str), "PING_RESP: MQTT broker\n");
                        putsUart0(str);
                        mqttMcb.mqttEvent = NOEVENT;
                }
            } else {
                /*wait*/
            }
            
        }break;

        case NOEVENT:
        {
            mqttHandleAllRxMsgs(ether);
        }break;
        default: break;
    }
}

/**
 * @brief mqttHandleAllRxMsgs
 * 
 * @param ether 
 */
void mqttHandleAllRxMsgs(etherHeader *ether)
{
    char str[35];

    if(!mqttRxBuffer.receviedData) { 
        /*Do nothing */
        return ;
    }

    mqttRxBuffer.receviedData = false;
    mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;

    switch(mqtt->controlPacket) {

        case MQ_CONNECT:
        {
            mqttHandleConnectServer(ether);
            mqttConnAck(ether);
        }break;

        case MQ_PUBLISH:
        {
            /*only process if the connection is alive*/
            if(!mqttGetConnState()) {return;}

            mqttHandlePubServer(ether);
            mqttPubAck(ether);
        }break;

        case MQ_PINGREQ:
        {
            /*if it's here, ping request packet is detected*/
            startOneshotTimer(&mqttKeepAliveTimeSrvCb,keepAliveSrvTimer);
            mqttPingResp(ether);

        }break;

        case MQ_DISCONNECT:
        {
            stopTimer(mqttKeepAliveTimeSrvCb);
            snprintf(str, sizeof(str), "DISCONNECT: from server received");
            putsUart0(str);
            mqttLogDisConnectEvent();

        }break;

        case MQ_SUBSCRIBE:
        {
            /*only process if the connection is alive*/
            if(!ismqttConnected()) {return; }

            mqttHandleSubServer(ether);
            mqttSubAck(ether);

        }break;

        default: break;
        
    } 
}

/**
 * @brief mqttHandlePubServer
 * 
 * @param ether 
 */
void mqttHandlePubServer(etherHeader *ether)
{
    char str[35];
    uint8_t *varHdrPtr;
    uint16_t topicSize = 0;
    uint8_t dataSize = 0;
    uint8_t i;
    bool flag = false;

    mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;

    varHdrPtr = mqtt->data;

    topicSize = *(varHdrPtr + 0);
    topicSize = (topicSize << 8) | *(varHdrPtr + 1);

    varHdrPtr += (uint8_t)2;

    strncpy(str,(char*)varHdrPtr,topicSize);

    for(i = 0; i < MQTT_NO_SUB_TOPICS; i++) {
        if(strcmp(str,mqttCurSubTopics[i] == 0)) {
            flag = true;
            break;
        }
    }

    if(!flag) {
        snprintf(str, sizeof(str), "waiting...\n ");
        putsUart0(str);
    } else {
        snprintf(str, sizeof(str), "PUBLISH from server:");
        putsUart0(str);
        memset(str,0,sizeof(str));
        strncpy(str,(char*)varHdrPtr,topicSize);
        putsUart0(str);
        putsUart0("\n");

        varHdrPtr += topicSize; /*topicsize + no msgId?? */

        dataSize = mqtt->remLength - 2 - topicSize; /* topiclensize(2bytes) - topiclenValue*/

        putsUart0("data: ");
        memset(str,0,sizeof(str));
        strncpy(str,(char*)varHdrPtr,dataSize);
        putsUart0(str);
        putsUart0("\n");
    }
}

/**
 * @brief mqttHandleSubServer
 * 
 * @param ether 
 */
void mqttHandleSubServer(etherHeader *ether)
{
    char str[35];
    uint8_t *varHdrPtr;
    uint16_t topicSize = 0;
    uint16_t packetId = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;

    varHdrPtr = mqtt->data;

    packetId = *(varHdrPtr + 0);
    packetId = (packetId << 8) | *(varHdrPtr + 1);

    varHdrPtr += (uint8_t)2;

    topicSize = *(varHdrPtr + 0);
    topicSize = (topicSize << 8) | *(varHdrPtr + 1);

    varHdrPtr += (uint8_t)2;

    snprintf(str, sizeof(str), "SUBSCRIBE: from server ");
    putsUart0(str);
    strncpy(str,(char*)varHdrPtr,topicSize);
    putsUart0(str);
    putsUart0("\n");

    /*TODO do we need to display data?*/

}

/**
 * @brief mqttHandleConnectServer
 * 
 * @param ether 
 */
void mqttHandleConnectServer(etherHeader *ether)
{
    char str[60];
    uint8_t *varHdrPtr;

    mqttHeader *mqtt = (mqttHeader*)mqttRxBuffer.data;

    varHdrPtr = mqtt->data;

    keepAliveSrvTimer = *(varHdrPtr+8);
    keepAliveSrvTimer = (keepAliveSrvTimer << 8) | *(varHdrPtr+9);

    startOneshotTimer(mqttKeepAliveTimeSrvCb,keepAliveSrvTimer);
    mqttServerConnectStatus = true;

    snprintf(str, sizeof(str), "CONNECT: connect session started with" PRIu16" keep alive timer\n",keepAliveSrvTimer);
    putsUart0(str);
}

/**
 * @brief mqttKeepAliveTimeSrvCb
 * 
 * @return _callback 
 */
_callback mqttKeepAliveTimeSrvCb()
{
    mqttServerConnectStatus = false;
}
/**
 * @brief mqttKeepAliveTimeCliCb
 * 
 * @return _callback 
 */
_callback mqttKeepAliveTimeCliCb()
{
    mqttMcb.mqttEvent = PING_REQUEST;
    mqttMcb.topicSize = 0;
    mqttMcb.dataSize = 0;
    mqttMcb.totalSize = 0;
}

bool ismqttConnected(void)
{
    return mqttServerConnectStatus;
}
/**
 * @brief mqttPubAck
 * 
 * @param ether 
 */
void mqttPubAck(etherHeader *ether)
{
    uint8_t *varHdrPtr;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_PUBACK;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_FIRE_FORGET;
    mqtt->retain = 1;

    varHdrPtr = mqtt->data;

    /*packet Identifier*/
    *(varHdrPtr + 0)= (mqttPktId & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttPktId & 0x00FF);
    varLen += 2;

    /*message length*/
    mqtt->remLength =  varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);

}

/**
 * @brief mqttSubAck
 * 
 * @param ether 
 */
void mqttSubAck(etherHeader *ether)
{
    uint8_t *varHdrPtr;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_SUBACK;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_FIRE_FORGET;
    mqtt->retain = 1;

    varHdrPtr = mqtt->data;

    /*packet Identifier*/
    *(varHdrPtr + 0)= (mqttPktId & 0xFF00) >> 8;
    *(varHdrPtr+ 1) = (uint8_t)(mqttPktId & 0x00FF);
    varLen += 2;

    /*success qos 0*/
    *(varHdrPtr + 0)= 0;
    varLen += 1;
    
    /*message length*/
    mqtt->remLength =  varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);

}

/**
 * @brief mqttConnAck
 * 
 * @param ether 
 */
void mqttConnAck(etherHeader *ether)
{
    uint8_t *varHdrPtr;
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_CONNACK;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_FIRE_FORGET;
    mqtt->retain = 0;

    varHdrPtr = mqtt->data;

    /*conn ack flags*/
    *(varHdrPtr + 0)= 0;
    *(varHdrPtr+ 1) = 0;
    varLen += 2;

    /*message length*/
    mqtt->remLength =  varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/**
 * @brief mqttPingResp
 * 
 * @param ether 
 */
void mqttPingResp(etherHeader *ether)
{
    uint16_t varLen = 0;

    mqttHeader *mqtt = (mqttHeader*)mqttMcb.data;
    mqtt->controlPacket = MQ_PINGRESP;
    mqtt->dup = 0; /*first try*/
    mqtt->qosLevel = MQ_FIRE_FORGET;
    mqtt->retain = 0;
    
    /*message length*/
    mqtt->remLength =  varLen;
    mqttMcb.totalSize = sizeof(mqttHeader) + varLen;

    mqttSetTxStatus(true);
}

/**
 * @brief mqttGetTxStatus
 * 
 * @return true 
 * @return false 
 */
bool mqttGetTxStatus(void)
{
    return mqttMcb.initiateTrans;
}

/**
 * @brief mqttSetTxStatus
 * 
 * @param status 
 */
void mqttSetTxStatus(bool status)
{
    mqttMcb.initiateTrans = status;
}

/**
 * @brief mqttGetTxData
 * 
 * @param data 
 * @param size 
 */
void mqttGetTxData(uint8_t *data, uint16_t *size)
{
    uint8_t i;

    for(i = 0; i < mqttMcb.totalSize; i++)
    {
        data[i] = mqttMcb.data[i];
    }
    *size = mqttMcb.totalSize;
}

/**
 * @brief isMqtt
 * 
 * @param ether 
 * @return true 
 * @return false 
 */
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

/**
 * @brief mqttSetRxData
 * 
 * @param ether 
 */
void mqttSetRxData(etherHeader *ether)
{
    uint8_t i;
    char str[30];

    tcpHeader *tcp = (tcpHeader*)getTcpHeader(ether);

    uint8_t *mqtt = (uint8_t*)tcp->data;
    uint8_t remlength = mqtt[1] + sizeof(mqttHeader) ;

    for(i = 0; i < remlength; i++)
    {
        mqttRxBuffer.data[i] = mqtt[i];
    }
    mqttRxBuffer.receviedData = true;
}

/**
 * @brief mqttInit
 * 
 */
void mqttInit(void)
{
    /* create a socket and bind it to initiate socket communication 
        when triggered
    */
    tcpCreateSocket(MQTT_PORT);
}

/**
 * @brief mqttGetCurrState
 * 
 * @return uint8_t 
 */
uint8_t mqttGetCurrState(void)
{
    return (uint8_t)mqttMcb.mqttEvent;
}

/**
 * @brief mqttSetCurrState
 * 
 * @param val 
 */
void mqttSetCurrState(mqttEvent_t val)
{
    mqttMcb.mqttEvent = val;
}

/**
 * @brief mqttGetConnState
 * 
 * @return uint8_t 
 */
uint8_t mqttGetConnState(void)
{
    return mqttConnStatus;
}

/**
 * @brief mqttSetConnState
 * 
 * @param val 
 */
void mqttSetConnState(uint8_t val)
{
    mqttConnStatus = val;
}
/**
 * @brief mqttConTimerCb
 * 
 * @return _callback 
 */
_callback mqttConTimerCb()
{
    mqttConTimerFlag = true;
}
