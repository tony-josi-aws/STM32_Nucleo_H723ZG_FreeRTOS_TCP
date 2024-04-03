#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

#define echoNUM_ECHO_CLIENTS				1
#define echoTCP_ECHO_SERVER_PORT			5050
#define configTCP_ECHO_SERVER_ADDR			"192.168.0.100"

/* The size of the buffers is a multiple of the MSS - the length of the data
sent is a pseudo random size between 20 and echoBUFFER_SIZES. */
#define echoBUFFER_SIZE_MULTIPLIER	( 3 )
#define echoBUFFER_SIZES			( ipconfigTCP_MSS * echoBUFFER_SIZE_MULTIPLIER )

/* Rx and Tx time outs are used to ensure the sockets do not wait too long for
missing data. */
static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 4000 );
static const TickType_t xSendTimeOut = pdMS_TO_TICKS( 2000 );

/* Counters for each created task - for inspection only. */
static uint32_t ulTxRxCycles[ echoNUM_ECHO_CLIENTS ]  = { 0 },
				ulTxRxFailures[ echoNUM_ECHO_CLIENTS ] = { 0 },
				ulConnections[ echoNUM_ECHO_CLIENTS ] = { 0 };

/* The echo tasks create a socket, send out a number of echo requests, listen
for the echo reply, then close the socket again before starting over.  This
delay is used between each iteration to ensure the network does not get too
congested. */
#define echoLOOP_DELAY	( ( TickType_t ) 500 / portTICK_PERIOD_MS )

static void prvEchoClientTask( void *pvParameters );

/* Rx and Tx buffers for each created task. */
static char cTxBuffers[ echoNUM_ECHO_CLIENTS ][ echoBUFFER_SIZES ],
			cRxBuffers[ echoNUM_ECHO_CLIENTS ][ echoBUFFER_SIZES ];

void vStartTCPEchoClientTasks_SingleTasks( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority )
{
BaseType_t x;

	/* Create the echo client tasks. */
	for( x = 0; x < echoNUM_ECHO_CLIENTS; x++ )
	{
		xTaskCreate( 	prvEchoClientTask,	/* The function that implements the task. */
						"Echo0",			/* Just a text name for the task to aid debugging. */
						usTaskStackSize,	/* The stack size is defined in FreeRTOSIPConfig.h. */
						( void * ) x,		/* The task parameter, not used in this case. */
						uxTaskPriority,		/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
						NULL );				/* The task handle is not used. */
	}
}

/*-----------------------------------------------------------*/
#if 0
static BaseType_t prvCreateTxData( char *cBuffer, uint32_t ulBufferLength )
{
BaseType_t lCharactersToAdd, lCharacter;
char cChar = '0';
const BaseType_t lMinimumLength = 60;
uint32_t ulRandomNumber;

	/* Randomise the number of characters that will be sent in the echo
	request. */
	do
	{
		extern BaseType_t xApplicationGetRandomNumber( uint32_t *pulValue );
        ( void ) xApplicationGetRandomNumber( &ulRandomNumber );
		lCharactersToAdd = ulRandomNumber % ( ulBufferLength - 20UL );

	} while ( ( lCharactersToAdd == 0 ) || ( lCharactersToAdd < lMinimumLength ) ); /* Must be at least enough to add the unique text to the start of the string later. */

	/* Fill the buffer. */
	for( lCharacter = 0; lCharacter < lCharactersToAdd; lCharacter++ )
	{
		cBuffer[ lCharacter ] = cChar;
		cChar++;

		if( cChar > '~' )
		{
			cChar = '0';
		}
	}

	return lCharactersToAdd;
}
#endif

/*-----------------------------------------------------------*/

static void prvEchoClientTask( void *pvParameters )
{
	Socket_t xSocket;
	struct freertos_sockaddr xEchoServerAddress;
	int32_t lLoopCount = 0UL;
	const int32_t lMaxLoopCount = 1000;
	volatile uint32_t ulTxCount = 0UL;
	BaseType_t xReceivedBytes, xReturned, xInstance;
	BaseType_t lTransmitted, lStringLength;
	char *pcTransmittedString, *pcReceivedString;
	WinProperties_t xWinProps;
	TickType_t xTimeOnEntering;
    BaseType_t xFamily = FREERTOS_AF_INET;
    uint8_t ucIPType = ipTYPE_IPv4;

	/* Fill in the buffer and window sizes that will be used by the socket. */
	xWinProps.lTxBufSize = 6 * ipconfigTCP_MSS;
	xWinProps.lTxWinSize = 3;
	xWinProps.lRxBufSize = 6 * ipconfigTCP_MSS;
	xWinProps.lRxWinSize = 3;

	/* This task can be created a number of times.  Each instance is numbered
	to enable each instance to use a different Rx and Tx buffer.  The number is
	passed in as the task's parameter. */
	xInstance = ( BaseType_t ) pvParameters;

	/* Point to the buffers to be used by this instance of this task. */
	pcTransmittedString = &( cTxBuffers[ xInstance ][ 0 ] );
	pcReceivedString = &( cRxBuffers[ xInstance ][ 0 ] );

    {
        BaseType_t rc = FreeRTOS_inet_pton(FREERTOS_AF_INET6, configTCP_ECHO_SERVER_ADDR, (void*)xEchoServerAddress.sin_address.xIP_IPv6.ucBytes);
        if (rc == pdPASS)
        {
            xFamily = FREERTOS_AF_INET6;
            ucIPType = ipTYPE_IPv6;
        }
        else
        {
            rc = FreeRTOS_inet_pton(FREERTOS_AF_INET4, configTCP_ECHO_SERVER_ADDR, (void*) &(xEchoServerAddress.sin_address.ulIP_IPv4));
            configASSERT(rc == pdPASS);
            xFamily = FREERTOS_AF_INET4;
            ucIPType = ipTYPE_IPv4;
        }
    }

    xEchoServerAddress.sin_len = sizeof(xEchoServerAddress);
    xEchoServerAddress.sin_port = FreeRTOS_htons(echoTCP_ECHO_SERVER_PORT);
    xEchoServerAddress.sin_family = xFamily;

	(void) ucIPType;

	for( ;; )
	{
		/* Create a TCP socket. */
		xSocket = FreeRTOS_socket( xFamily, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
		configASSERT( xSocket != FREERTOS_INVALID_SOCKET );

		/* Set a time out so a missing reply does not cause the task to block
		indefinitely. */
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

		/* Set the window and buffer sizes. */
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps,	sizeof( xWinProps ) );

		/* Connect to the echo server. */
		if( FreeRTOS_connect( xSocket, &xEchoServerAddress, sizeof( xEchoServerAddress ) ) == 0 )
		{
			ulConnections[ xInstance ]++;

			/* Send a number of echo requests. */
			for( lLoopCount = 0; lLoopCount < lMaxLoopCount; lLoopCount++ )
			{
				/* Create the string that is sent to the echo server. */
				sprintf( pcTransmittedString, "TxRx message number %lu", ulTxCount );
				lStringLength = strlen(pcTransmittedString);
				ulTxCount++;

				/* Send the string to the socket. */
				lTransmitted = FreeRTOS_send(	xSocket,						/* The socket being sent to. */
												( void * ) pcTransmittedString,	/* The data being sent. */
												strlen(pcTransmittedString),					/* The length of the data being sent. */
												0 );							/* No flags. */

				if( lTransmitted < 0 )
				{
					/* Error? */
					break;
				}

				/* Clear the buffer into which the echoed string will be
				placed. */
				memset( ( void * ) pcReceivedString, 0x00, echoBUFFER_SIZES );
				xReceivedBytes = 0;

				/* Receive data echoed back to the socket. */
				while( xReceivedBytes < lTransmitted )
				{
					xReturned = FreeRTOS_recv( xSocket,								/* The socket being received from. */
											&( pcReceivedString[ xReceivedBytes ] ),/* The buffer into which the received data will be written. */
											 lStringLength - xReceivedBytes,		/* The size of the buffer provided to receive the data. */
											 0 );									/* No flags. */

					if( xReturned < 0 )
					{
						/* Error occurred.  Latch it so it can be detected
						below. */
						xReceivedBytes = xReturned;
						break;
					}
					else if( xReturned == 0 )
					{
						/* Timed out. */
						break;
					}
					else
					{
						/* Keep a count of the bytes received so far. */
						xReceivedBytes += xReturned;
					}
				}

				/* If an error occurred it will be latched in xReceivedBytes,
				otherwise xReceived bytes will be just that - the number of
				bytes received from the echo server. */
				if( xReceivedBytes > 0 )
				{
					/* Compare the transmitted string to the received string. */
					configASSERT( strncmp( pcReceivedString, pcTransmittedString, lTransmitted ) == 0 );
					if( strncmp( pcReceivedString, pcTransmittedString, lTransmitted ) == 0 )
					{
						/* The echo reply was received without error. */
						ulTxRxCycles[ xInstance ]++;
					}
					else
					{
						/* The received string did not match the transmitted
						string. */
						ulTxRxFailures[ xInstance ]++;
						break;
					}
				}
				else if( xReceivedBytes < 0 )
				{
					/* FreeRTOS_recv() returned an error. */
					break;
				}
				else
				{
					/* Timed out without receiving anything? */
					break;
				}
			}

			/* Finished using the connected socket, initiate a graceful close:
			FIN, FIN+ACK, ACK. */
			FreeRTOS_shutdown( xSocket, FREERTOS_SHUT_RDWR );

			/* Expect FreeRTOS_recv() to return an error once the shutdown is
			complete. */
			xTimeOnEntering = xTaskGetTickCount();
			do
			{
				xReturned = FreeRTOS_recv( xSocket,	/* The socket being received from. */
					&( pcReceivedString[ 0 ] ),		/* The buffer into which the received data will be written. */
					echoBUFFER_SIZES,				/* The size of the buffer provided to receive the data. */
					0 );

				if( xReturned < 0 )
				{
					break;
				}

			} while( ( xTaskGetTickCount() - xTimeOnEntering ) < xReceiveTimeOut );
		}

		/* Close this socket before looping back to create another. */
		FreeRTOS_closesocket( xSocket );

		/* Pause for a short while to ensure the network is not too
		congested. */
		vTaskDelay( echoLOOP_DELAY );
	}
}
/*-----------------------------------------------------------*/
