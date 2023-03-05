// IP Library
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

#include <inttypes.h>
#include <stdio.h>
#include "ip_custom_layer.h"
#include "arp.h"
#include "tcp.h"
#include "timer.h"
#include "uart0.h"
// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
bool initiateArpRequest = false;
ipEvent_t ipEventIcb = IP_NOEVENT; 
static uint8_t arpResponseTimeout = false;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

/**
 * @brief ipHandlerTx
 * 
 * @param ether 
 */
void ipHandlerTx(etherHeader *ether)
{
    uint8_t localIpAddr[IP_ADD_LENGTH] = {0};
    uint8_t remoteIpAddr[IP_ADD_LENGTH] = {0};

    switch (ipEventIcb)
    {
        case IP_ARP_REQ_RESP:
        {
            if((arpResponseTimeout == 0)
                || (arpResponseTimeout == 2))
            {

                arpResponseTimeout = 1;
                getIpAddress(localIpAddr);
                getIpMqttBrokerAddress(remoteIpAddr);
                sendArpRequest(ether,localIpAddr,remoteIpAddr);
                /*enable timer*/
                startPeriodicTimer(arpRespTimeoutCb,2);
            }
        }
        break;
        
        case IP_NOEVENT:
        default:
            break;
    }
}

/**
 * @brief ipHandlerRx
 * 
 * @param ether 
 */
void ipHandlerRx(etherHeader *ether)
{
    char str[30];

    switch (ipEventIcb)
    {
        case IP_ARP_REQ_RESP:
        {
            uint8_t remoteMac[HW_ADD_LENGTH] = {0};
            arpPacket *arp = (arpPacket*)ether->data;
            uint8_t i = 0;

            while (i < HW_ADD_LENGTH)
            {
                remoteMac[i] = arp->sourceAddress[i];
                i++;
            }
            setIpMqttMacBrokerAddress(remoteMac);
            arpResponseTimeout = 0;
            stopTimer(arpRespTimeoutCb);
            ipEventIcb = IP_NOEVENT;
            snprintf(str, sizeof(str), "remote mac: ");
            putsUart0(str);
            for (i = 0; i < HW_ADD_LENGTH; i++)
            {
                snprintf(str, sizeof(str), "%02x", remoteMac[i]);
                putsUart0(str);
                if (i < HW_ADD_LENGTH-1)
                    putcUart0(':');
            }
            putcUart0('\n');
        }
        break;
        
        case IP_NOEVENT:
        default:
            break;
    }
}

/**
 * @brief Set the Ip Mqtt Mac Broker Address object
 * 
 * @param mac 
 */
void setIpMqttMacBrokerAddress(uint8_t mac[HW_ADD_LENGTH])
{
    uint8_t i;
    for (i = 0; i < HW_ADD_LENGTH; i++)
        socketConns[0].s.remoteHwAddress[i] = mac[i];
}

/**
 * @brief Get the Ip Current Event Status object
 * 
 * @return ipEvent_t 
 */
ipEvent_t getIpCurrentEventStatus(void)
{
    return ipEventIcb;
}

/**
 * @brief arpRespTimeoutCb
 * 
 * @return _callback 
 */
_callback arpRespTimeoutCb()
{
    arpResponseTimeout = 2;
}

/**
 * @brief isIpArpResponse
 * 
 * @param ether 
 * @return true 
 * @return false 
 */
bool isIpArpResponse(etherHeader *ether)
{
    arpPacket *arp = (arpPacket*)ether->data;
    bool ok;
    uint8_t i = 0;
    uint8_t localIpAddress[4];
    ok = (ether->frameType == htons(TYPE_ARP));
    getIpAddress(localIpAddress);
    while (ok && (i < IP_ADD_LENGTH))
    {
        ok = (arp->destIp[i] == localIpAddress[i]);
        i++;
    }
    if (ok)
        ok = (arp->op == htons(2));
    return ok;
}
