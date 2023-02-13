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

/* Message Type */
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


typedef struct _mqttHeader
{
    uint8_t messageType:4;
    uint8_t dup:1;
    uint8_t qosLevel:2;
    uint8_t retain:1;
    uint8_t remLength; /* 1 to 4 bytes maybe uint32_t need to check while processing data */
    uint8_t data[0];
}mqttHeader;

typedef enum 
{
    NOEVENT,
    PUBLISH,
    SUBSCRIBE,
    CONNECT,
    DISCONNECT
}mqttEvent_t;

typedef struct 
{
    mqttEvent_t mqttEvent;
    uint8_t data;
    uint8_t topic;
}mqttMcb_t;

void mqttPublish(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttSubscribe(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttUnsubscribe(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttConnect(etherHeader *ether, uint8_t *data, uint16_t size);
void mqttDisconnect(etherHeader *ether, uint8_t *data, uint16_t size);

void mqttLogPublishEvent(uint8_t topic, uint8_t data);

#endif /* MQTT_H_ */
