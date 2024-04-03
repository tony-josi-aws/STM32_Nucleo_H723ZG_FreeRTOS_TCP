/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_ND.h"
#include "FreeRTOS_Sockets.h"

/* Logging includes. */
#include "logging.h"

/* Demo definitions. */
#define mainCLI_TASK_STACK_SIZE             512
#define mainCLI_TASK_PRIORITY               (tskIDLE_PRIORITY)

#define mainUSER_COMMAND_TASK_STACK_SIZE    2048
#define mainUSER_COMMAND_TASK_PRIORITY      (tskIDLE_PRIORITY)
/*-----------------------------------------------------------*/
/*-------------  ***  DEMO DEFINES   ***   ------------------*/
/*-----------------------------------------------------------*/

/* Set the USE_IPv6_END_POINTS macro to 1 or 0 to enable IPv6
 * endpoints.
 */
#define USE_IPv6_END_POINTS                 1

#if ( ipconfigUSE_IPv6 != 0 && USE_IPv6_END_POINTS != 0 && ipconfigUSE_IPv4 != 0 )
    #define TOTAL_ENDPOINTS                 3
#elif ( ipconfigUSE_IPv4 == 0 && ipconfigUSE_IPv6 != 0 )
    #define TOTAL_ENDPOINTS                 2
#else
    #define TOTAL_ENDPOINTS                 1
#endif /* ( ipconfigUSE_IPv6 != 0 && USE_IPv6_END_POINTS != 0 ) */



/* Set the following constants to 1 or 0 to define which tasks to include and
 * exclude:
 *
 * mainCREATE_TCP_ECHO_TASKS_SINGLE:  When set to 1 a set of tasks are created that
 * send TCP echo requests to the defined echo port (echoTCP_ECHO_SERVER_PORT), then wait for and
 * verify the echo reply, from within the same task (Tx and Rx are performed in the
 * same RTOS task).  The IP address of the echo server must be configured using the
 * configTCP_ECHO_SERVER_ADDR macro
 *
 * mainCREATE_UDP_ECHO_TASKS_SINGLE:  When set to 1 a task is created that sends data
 * to the address configECHO_SERVER_ADDR_STRING (IPv4/Ipv6) and port (configECHO_SERVER_PORT) 
 * where it is expected to echo back the data, which, the created tasks receives.
 *
 */
#define mainCREATE_TCP_ECHO_TASKS_SINGLE              1 /* 1 */
#define mainCREATE_UDP_ECHO_TASKS_SINGLE              1

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

#define SYS_LOG_PRINT_BUFFER_SIZE           256

/* Logging module configuration. */
#define mainLOGGING_TASK_STACK_SIZE         256
#define mainLOGGING_TASK_PRIORITY           (tskIDLE_PRIORITY + 1)
#define mainLOGGING_QUEUE_LENGTH            100

#define mainMAX_UDP_RESPONSE_SIZE           1024

/*-----------------------------------------------------------*/

uint32_t ulTim7Tick = 0;

BaseType_t xEndPointCount = 0;
BaseType_t xUpEndPointCount = 0;

static BaseType_t xTasksAlreadyCreated = pdFALSE;

/*-----------------------------------------------------------*/

void app_main( void )
{
    BaseType_t xRet;
    const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
    const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
    const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
    const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };
    const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };

    xRet = xLoggingTaskInitialize( mainLOGGING_TASK_STACK_SIZE,
                                   mainLOGGING_TASK_PRIORITY,
                                   mainLOGGING_QUEUE_LENGTH );
    configASSERT( xRet == pdPASS );

    configPRINTF( ( "Calling FreeRTOS_IPInit...\n" ) );


  	static NetworkInterface_t xInterfaces[1];
   	static NetworkEndPoint_t xEndPoints[4];
	//memcpy(ipLOCAL_MAC_ADDRESS, ucMACAddress, sizeof ucMACAddress);

	/* Initialize the network interface.*/
    #if defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
        /* Initialize the interface descriptor for WinPCap. */
        extern NetworkInterface_t * pxSTM32H_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                         NetworkInterface_t * pxInterface );
        pxSTM32H_FillInterfaceDescriptor(0, &(xInterfaces[0]));

        /* === End-point 0 === */
        #if ( ipconfigUSE_IPv4 != 0 )
            {
                FreeRTOS_FillEndPoint(&(xInterfaces[0]), &(xEndPoints[xEndPointCount]), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);
                #if ( ipconfigUSE_DHCP != 0 )
                {
                    /* End-point 0 wants to use DHCPv4. */
                    xEndPoints[xEndPointCount].bits.bWantDHCP = pdTRUE; // pdFALSE; // pdTRUE;
                }
                #endif /* ( ipconfigUSE_DHCP != 0 ) */

                xEndPointCount += 1;
            }
        #endif /* ( ipconfigUSE_IPv4 != 0 ) */

        /*
        *     End-point-1 : public
        *     Network: 2001:470:ed44::/64
        *     IPv6   : 2001:470:ed44::4514:89d5:4589:8b79/128
        *     Gateway: fe80::ba27:ebff:fe5a:d751  // obtained from Router Advertisement
        */
        #if ( ipconfigUSE_IPv6 != 0 && USE_IPv6_END_POINTS != 0 )
            {
                IPv6_Address_t xIPAddress;
                IPv6_Address_t xPrefix;
                IPv6_Address_t xGateWay;

                FreeRTOS_inet_pton6( "2001:470:ed44::", xPrefix.ucBytes );

                FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
                FreeRTOS_inet_pton6( "fe80::ba27:ebff:fe5a:d751", xGateWay.ucBytes );

                FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[ 0 ] ),
                                            &( xEndPoints[ xEndPointCount ] ),
                                            &( xIPAddress ),
                                            &( xPrefix ),
                                            64uL, /* Prefix length. */
                                            &( xGateWay ),
                                            NULL, /* pxDNSServerAddress: Not used yet. */
                                            ucMACAddress );
                FreeRTOS_inet_pton6( "2001:4860:4860::8888", xEndPoints[ xEndPointCount ].ipv6_settings.xDNSServerAddresses[ 0 ].ucBytes );
                FreeRTOS_inet_pton6( "fe80::1", xEndPoints[ xEndPointCount ].ipv6_settings.xDNSServerAddresses[ 1 ].ucBytes );
                FreeRTOS_inet_pton6( "2001:4860:4860::8888", xEndPoints[ xEndPointCount ].ipv6_defaults.xDNSServerAddresses[ 0 ].ucBytes );
                FreeRTOS_inet_pton6( "fe80::1", xEndPoints[ xEndPointCount ].ipv6_defaults.xDNSServerAddresses[ 1 ].ucBytes );

                #if ( ipconfigUSE_RA != 0 )
                    {
                        /* End-point 1 wants to use Router Advertisement */
                        xEndPoints[ xEndPointCount ].bits.bWantRA = pdTRUE;
                    }
                #endif /* #if( ipconfigUSE_RA != 0 ) */
                #if ( ipconfigUSE_DHCPv6 != 0 )
                    {
                        /* End-point 1 wants to use DHCPv6. */
                        xEndPoints[ xEndPointCount ].bits.bWantDHCP = pdTRUE;
                    }
                #endif /* ( ipconfigUSE_DHCPv6 != 0 ) */

                xEndPointCount += 1;

            }

            /*
            *     End-point-3 : private
            *     Network: fe80::/10 (link-local)
            *     IPv6   : fe80::d80e:95cc:3154:b76a/128
            *     Gateway: -
            */
            {
                IPv6_Address_t xIPAddress;
                IPv6_Address_t xPrefix;

                FreeRTOS_inet_pton6( "fe80::", xPrefix.ucBytes );
                FreeRTOS_inet_pton6( "fe80::7009", xIPAddress.ucBytes );

                FreeRTOS_FillEndPoint_IPv6(
                    &( xInterfaces[ 0 ] ),
                    &( xEndPoints[ xEndPointCount ] ),
                    &( xIPAddress ),
                    &( xPrefix ),
                    10U,  /* Prefix length. */
                    NULL, /* No gateway */
                    NULL, /* pxDNSServerAddress: Not used yet. */
                    ucMACAddress );

                xEndPointCount += 1;
            }

        #endif /* ( ipconfigUSE_IPv6 != 0 && USE_IPv6_END_POINTS != 0 ) */

        FreeRTOS_IPInit_Multi();
    #else
        /* Using the old /single /IPv4 library, or using backward compatible mode of the new /multi library. */
        FreeRTOS_debug_printf(("FreeRTOS_IPInit\r\n"));
        FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);
    #endif /* defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */

    /* Start the RTOS scheduler. */
    vTaskStartScheduler();

    /* Infinite loop */
    for(;;)
    {
        
    }
}


#if defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
	void vApplicationIPNetworkEventHook_Multi( eIPCallbackEvent_t eNetworkEvent, struct xNetworkEndPoint* pxEndPoint )
#else
	void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent)
#endif /* defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */
{
    /* If the network has just come up...*/
    if( eNetworkEvent == eNetworkUp )
    {


        xUpEndPointCount += 1;

        if( xUpEndPointCount >= TOTAL_ENDPOINTS )
        {

            /* Create the tasks that use the IP stack if they have not already been
            * created. */
            if( xTasksAlreadyCreated == pdFALSE )
            {
                xTasksAlreadyCreated = pdTRUE;

                #if ( ipconfigUSE_IPv4 != 0 )

                    #if mainCREATE_TCP_ECHO_TASKS_SINGLE

                        void vStartTCPEchoClientTasks_SingleTasks( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority );
                        vStartTCPEchoClientTasks_SingleTasks(mainCLI_TASK_STACK_SIZE, mainCLI_TASK_PRIORITY);

                    #endif

                #endif

                #if ( mainCREATE_UDP_ECHO_TASKS_SINGLE == 1 )
                    {
                        void vStartUDPEchoClientTasks_SingleTasks(uint16_t usTaskStackSize, UBaseType_t uxTaskPriority);
                        vStartUDPEchoClientTasks_SingleTasks( mainCLI_TASK_STACK_SIZE, mainCLI_TASK_PRIORITY );
                    }
                #endif

            }

        }

        #if defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )

        	extern void showEndPoint( NetworkEndPoint_t * pxEndPoint );
            showEndPoint( pxEndPoint );
        
        #else

            char cBuffer[ 16 ];
            uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
        
            /* Print out the network configuration, which may have come from a DHCP
            * server. */
            #if defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
                FreeRTOS_GetEndPointConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress, pxNetworkEndPoints );
            #else
                FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
            #endif /* defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */

            FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
            configPRINTF( ( "IP Address: %s\n", cBuffer ) );

            FreeRTOS_inet_ntoa( ulNetMask, cBuffer );
            configPRINTF( ( "Subnet Mask: %s\n", cBuffer ) );

            FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );
            configPRINTF( ( "Gateway Address: %s\n", cBuffer ) );

            FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
            configPRINTF( ( "DNS Server Address: %s\n", cBuffer ) );
        
        #endif /* defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */

    }
    else if( eNetworkEvent == eNetworkDown )
    {

    }
}
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )

#if defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
	BaseType_t xApplicationDNSQueryHook_Multi( struct xNetworkEndPoint * pxEndPoint, const char * pcName )
#else
	BaseType_t xApplicationDNSQueryHook( const char * pcName )
#endif /* defined(ipconfigIPv4_BACKWARD_COMPATIBLE) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */
{
    BaseType_t xReturn = pdFAIL;

    /* Determine if a name lookup is for this node.  Two names are given
     * to this node: that returned by pcApplicationHostnameHook() and that set
     * by mainDEVICE_NICK_NAME. */
    if( strcasecmp( pcName, pcApplicationHostnameHook() ) == 0 )
    {
        xReturn = pdPASS;
    }
    return xReturn;
}

#endif /* ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 ) */

/*-----------------------------------------------------------*/

const char *pcApplicationHostnameHook( void )
{
    /* Assign the name "STM32Hxx" to this network node.  This function will be
     * called during the DHCP: the machine will be registered with an IP address
     * plus this name. */
    return "STM32Hxx";
}
/*-----------------------------------------------------------*/

BaseType_t xApplicationGetRandomNumber( uint32_t *pulValue )
{
BaseType_t xReturn;
	extern RNG_HandleTypeDef hrng;
    if( HAL_RNG_GenerateRandomNumber( &hrng, pulValue ) == HAL_OK )
    {
        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
                                             uint16_t usSourcePort,
                                             uint32_t ulDestinationAddress,
                                             uint16_t usDestinationPort )
{
    uint32_t ulReturn;

    ( void ) ulSourceAddress;
    ( void ) usSourcePort;
    ( void ) ulDestinationAddress;
    ( void ) usDestinationPort;

    xApplicationGetRandomNumber( &ulReturn );

    return ulReturn;
}


/*-----------------------------------------------------------*/

void vIncrementTim7Tick( void )
{
    ulTim7Tick++;
}
/*-----------------------------------------------------------*/

uint32_t ulGetTim7Tick( void )
{
    return ulTim7Tick;
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    /* If configCHECK_FOR_STACK_OVERFLOW is set to either 1 or 2 then this
     * function will automatically get called if a task overflows its stack. */
    ( void ) pxTask;
    ( void ) pcTaskName;
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* If configUSE_MALLOC_FAILED_HOOK is set to 1 then this function will
     * be called automatically if a call to pvPortMalloc() fails.  pvPortMalloc()
     * is called automatically when a task, queue or semaphore is created. */
    for( ;; );
}
/*-----------------------------------------------------------*/

time_t get_time( time_t * puxTime )
{
    time_t val = 0;
    return val;
}

int set_time( const time_t * t )
{
    return 0;
}
