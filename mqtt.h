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

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

/* Message Type/Control packet type */
#define MQ_CONNECT      (1u)
#define MQ_CONNACK      (2u)
#define MQ_PUBLISH      (3u)
#define MQ_PUBACK       (4u)
#define MQ_PUBREC       (5u)
#define MQ_PUBREL       (6u)
#define MQ_PUBCOMP      (7u)
#define MQ_SUBSCRIBE    (8u)
#define MQ_SUBACK       (9u)
#define MQ_UNSUBSCRIBE  (10u)
#define MQ_UNSUBACK     (11u)
#define MQ_PINGREQ      (12u)
#define MQ_PINGRESP     (13u)
#define MQ_DISCONNECT   (14u)
#define MQ_AUTH         (15u)

/* QOS value */
#define MQ_FIRE_FORGET  (0u)
#define MQ_ACK_DELIVERY (1u)
#define MQ_ASS_DELIVERY (2u)
#define MQ_RESERVED     (3u)

#define MQTT_SIZE       (60u)
#define MQTT_SRV_PORT   (8081u)
#define MQTT_PORT       (1883u)

#define MQTT_DISCONNECTED   0u
#define MQTT_CONNECTED      1u

#define MQTT_NO_SUB_TOPICS  (2u)
#define MQTT_TOPIC_LEN      (23u)

typedef struct _mqttHeader
{
    uint8_t retain:1;
    uint8_t qosLevel:2;
    uint8_t dup:1;
    uint8_t controlPacket:4;
    uint8_t remLength; /* 1 to 4 bytes maybe uint32_t need to check while processing data */
    uint8_t data[0];
}mqttHeader;

typedef enum 
{
    NOEVENT,
    PUBLISH,
    PUBLISH_WAIT,
    SUBSCRIBE,
    SUBSCRIBE_WAIT,
    UNSUBSCRIBE,
    UNSUBSCRIBE_WAIT,
    CONNECT,
    CONNECT_WAIT,
    DISCONNECT,
    DISCONNECT_WAIT,
    PING_REQUEST,
    PING_RESPONSE
}mqttEvent_t;

typedef struct _mqttMcb_t
{
    mqttEvent_t mqttEvent;
    uint16_t totalSize;
    uint16_t topicSize;
    uint16_t dataSize;
    char data[MQTT_SIZE];
    char args[MQTT_SIZE];
    bool initiateTrans;
}mqttMcb_t;

typedef struct 
{
    mqttEvent_t mqttEvent;
    char data[MQTT_SIZE];
    bool receviedData;
}mqttRxBf_t;

extern char *mqttState[];
extern char *mqttFsmState[];

void mqttPublish(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttSubscribe(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttUnsubscribe(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttConnect(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttDisconnect(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttPingRequest(etherHeader *ether, uint8_t *data, uint16_t size);

void mqttLogPublishEvent(char *data, uint16_t size, bool flag);
void mqttLogSubscribeEvent(char *data, uint16_t size);
void mqttLogUnSubscribeEvent(char *data, uint16_t size);
void mqttLogConnectEvent(void);
void mqttLogDisConnectEvent(void);
bool mqttGetTxStatus(void);
void mqttSetTxStatus(bool status);
void mqttGetTxData(uint8_t *data, uint16_t *size);
void mqttHandler(etherHeader *ether );
void mqttInit(void);
void mqttSetRxData(etherHeader *ether);
void appInit(void);
bool isMqtt(etherHeader *ether);
uint8_t mqttGetCurrState(void);
void mqttSetCurrState(mqttEvent_t val);
void mqttSetConnState(uint8_t val);
uint8_t mqttGetConnState(void);
void mqttPubAck(etherHeader *ether);
void mqttSubAck(etherHeader *ether);
void mqttHandlePubServer(etherHeader *ether);
void mqttHandleSubServer(etherHeader *ether);
void mqttHandleAllRxMsgs(etherHeader *ether);
void mqttHandleConnectServer(etherHeader *ether);
void mqttConnAck(etherHeader *ether);
void mqttPingResp(etherHeader *ether);
bool ismqttConnected(void);
void mqttCalcDynLength(uint8_t *ptr, uint32_t enclen, uint8_t *noOfBytes);

_callback mqttKeepAliveTimeSrvCb();
_callback mqttKeepAliveTimeCliCb();
_callback mqttConTimerCb();
#endif /* MQTT_H_ */
