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
#include "mqtt.h"
#include "timer.h"
#include "tcp.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
static mqttMcb_t mqttMcb = {NOEVENT,0,0};
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
void mqttPublish(etherHeader *ether, uint8_t *data, uint16_t size)
{
    //uint8_t data[10];

    //tcpConnect(0);

    mqttHeader *mqttData = (mqttHeader*)data;

    //mqttData->
}

/*TODO : might have to implement a state machine to split the larger data packets 
to feed into tcp/ip if it exceeds MTU
*/

void mqttLogPublishEvent(uint8_t topic, uint8_t data)
{
    mqttMcb.mqttEvent = PUBLISH;
    mqttMcb.data = data;
    mqttMcb.topic = topic;
}

void mqttHandler(etherHeader *ether )
{
    uint8_t dummyIpAddr[] = {1,2,3,4};
    switch (mqttMcb.mqttEvent)
    {
        case PUBLISH:
        {
            /*send the mqtt request after it gets connected */
            if(getTcpCurrState(0) == TCP_ESTABLISHED) {
                    /* send the the topic and data with mqtt message */
                    mqttMcb.mqttEvent = NOEVENT;
            } else if (getTcpCurrState(0) == TCP_CLOSED) {
                tcpCreateSocket(8181, dummyIpAddr);
                tcpConnect(0);
            }
            
        }break;

        case SUBSCRIBE:
        {

        }break;

        case CONNECT:
        {

        }break;

        case DISCONNECT:
        {

        }break;


        case NOEVENT:
        default: break;
    }
}
