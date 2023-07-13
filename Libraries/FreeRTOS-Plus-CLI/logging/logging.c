/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


/* Sanity check all the definitions required by this file are set. */
#ifndef configPRINT_STRING
    #error configPRINT_STRING( x ) must be defined in FreeRTOSConfig.h to use this logging file.  Set configPRINT_STRING( x ) to a function that outputs a string, where X is the string.  For example, #define configPRINT_STRING( x ) MyUARTWriteString( X )
#endif

#ifndef configLOGGING_MAX_MESSAGE_LENGTH
    #error configLOGGING_MAX_MESSAGE_LENGTH must be defined in FreeRTOSConfig.h to use this logging file.  configLOGGING_MAX_MESSAGE_LENGTH sets the size of the buffer into which formatted text is written, so also sets the maximum log message length.
#endif

/* A block time of 0 just means don't block. */
#define loggingDONT_BLOCK    0

/*
 * Wrapper function for vsnprintf to return the actual number of
 * characters written.
 *
 * From the documentation, the return value of vsnprintf is:
 * 1. In case of success i.e. when the complete string is successfully written
 *    to the buffer, the return value is the number of characters written to the
 *    buffer not counting the terminating null character.
 * 2. In case when the buffer is not large enough to hold the complete string,
 *    the return value is the number of characters that would have been written
 *    if the buffer was large enough.
 * 3. In case of encoding error, a negative number is returned.
 *
 * This wrapper function instead returns the actual number of characters
 * written in all cases:
 * 1. In case of success i.e. when the complete string is successfully written
 *    to the buffer, these wrapper returns the same value as from
 *    vsnprintf.
 * 2. In case when the buffer is not large enough to hold the complete string,
 *    the wrapper function returns the number of actual characters written
 *    (i.e. n - 1) as opposed to the number of characters that would have been
 *    written if the buffer was large enough.
 * 3. In case of encoding error, these wrapper functions return 0 to indicate
 *    that nothing was written as opposed to negative value from
 *    vsnprintf.
 */
static int vsnprintf_safe( char * s,
                           size_t n,
                           const char * format,
                           va_list arg );

/*-----------------------------------------------------------*/

/*
 * The task that actually performs the print output.  Using a separate task
 * enables the use of slow output, such as as a UART, without the task that is
 * outputting the log message having to wait for the message to be completely
 * written.  Using a separate task also serializes access to the output port.
 *
 * The structure of this task is very simple; it blocks on a queue to wait for
 * a pointer to a string, sending any received strings to a macro that performs
 * the actual output.  The macro is port specific, so implemented outside of
 * this file.  This version uses dynamic memory, so the buffer that contained
 * the log message is freed after it has been output.
 */
static void prvLoggingTask( void * pvParameters );

/*-----------------------------------------------------------*/

/*
 * The queue used to pass pointers to log messages from the task that created
 * the message to the task that will performs the output.
 */
static QueueHandle_t xQueue = NULL;

/*-----------------------------------------------------------*/

static int vsnprintf_safe( char * s,
                           size_t n,
                           const char * format,
                           va_list arg )
{
    int ret;

    ret = vsnprintf( s, n, format, arg );

    /* Check if the string was truncated and if so, update the return value
     * to reflect the number of characters actually written. */
    if( ret >= n )
    {
        /* Do not include the terminating NULL character to keep the behaviour
         * same as the standard. */
        ret = n - 1;
    }
    else if( ret < 0 )
    {
        /* Encoding error - Return 0 to indicate that nothing was written to the
         * buffer. */
        ret = 0;
    }
    else
    {
        /* Complete string was written to the buffer. */
    }

    return ret;
}

/*-----------------------------------------------------------*/

BaseType_t xLoggingTaskInitialize( uint16_t usStackSize,
                                   UBaseType_t uxPriority,
                                   UBaseType_t uxQueueLength )
{
    BaseType_t xReturn = pdFAIL;

    /* Ensure the logging task has not been created already. */
    if( xQueue == NULL )
    {
        /* Create the queue used to pass pointers to strings to the logging task. */
        xQueue = xQueueCreate( uxQueueLength, sizeof( char ** ) );

        if( xQueue != NULL )
        {
            if( xTaskCreate( prvLoggingTask, "Logging", usStackSize, NULL, uxPriority, NULL ) == pdPASS )
            {
                xReturn = pdPASS;
            }
            else
            {
                /* Could not create the task, so delete the queue again. */
                vQueueDelete( xQueue );
            }
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

static void prvLoggingTask( void * pvParameters )
{
    /* Disable unused parameter warning. */
    ( void ) pvParameters;

    char * pcReceivedString = NULL;

    for( ; ; )
    {
        /* Block to wait for the next string to print. */
        if( xQueueReceive( xQueue, &pcReceivedString, portMAX_DELAY ) == pdPASS )
        {
            configPRINT_STRING( pcReceivedString );

            vPortFree( ( void * ) pcReceivedString );
        }
    }
}

/*-----------------------------------------------------------*/

void vLoggingPrintf( const char * pcFormat, ... )
{
    size_t xLength = 0;
    char * pcPrintString = NULL;

    /* The queue is created by xLoggingTaskInitialize().  Check
     * xLoggingTaskInitialize() has been called. */
    configASSERT( xQueue );
    configASSERT( pcFormat != NULL );

    /* Allocate a buffer to hold the log message. */
    pcPrintString = pvPortMalloc( configLOGGING_MAX_MESSAGE_LENGTH );

    if( pcPrintString != NULL )
    {
        {
            va_list args;

            va_start( args, pcFormat );
            xLength = vsnprintf_safe( pcPrintString,
                                      configLOGGING_MAX_MESSAGE_LENGTH,
                                      pcFormat,
                                      args );
            va_end( args );
        }

        pcPrintString[ xLength ] = '\r';
        pcPrintString[ xLength + 1] = '\0';

        /* Send the string to the logging task for IO. */
        if( xQueueSend( xQueue, &pcPrintString, loggingDONT_BLOCK ) != pdPASS )
        {
            /* The buffer was not sent so must be freed again. */
            vPortFree( ( void * ) pcPrintString );
        }
    }
}

/*-----------------------------------------------------------*/
