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
static mqttMcb_t mqttMcb = {NOEVENT,0,0,false};
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

    mqttSetTxStatus(true);

    //mqttData->

    /*create a mqtt header and store it */
}

/*TODO : might have to implement a state machine to split the larger data packets 
to feed into tcp/ip if it exceeds MTU
*/

void mqttLogPublishEvent(uint8_t topic, uint8_t data)
{
    mqttMcb.mqttEvent = PUBLISH;
    mqttMcb.data[0] = data;
    mqttMcb.data[1] = topic;
    mqttMcb.size = 2;
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
                    mqttPublish(ether, mqttMcb.data , mqttMcb.size);
                    mqttMcb.mqttEvent = NOEVENT;
            } else if (getTcpCurrState(0) == TCP_CLOSED) {
               
                /* create a socket and bind it to initiate socket communication */ 
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

bool mqttGetTxStatus(void)
{
    return mqttMcb.initiateTrans;
}

void mqttSetTxStatus(bool status)
{
    mqttMcb.initiateTrans = status;
}

void mqttPubGetData(uint8_t *data, uint16_t *size)
{
    uint8_t i;

    for(i = 0; i < mqttMcb.size; i++)
    {
        data[i] = mqttMcb.data[i];
    }
    *size = mqttMcb.size;
}

void isMqtt(etherHeader *ether)
{

}
