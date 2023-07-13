#ifndef LOGGING_H
#define LOGGING_H

/* Kernel includes. */
#include "FreeRTOS.h"

/**
 * @brief Initialize the logging module.
 *
 * @param usStackSize Stack size for logging task.
 * @param uxPriority Priority of the logging task.
 * @param uxQueueLength Length of the queue holding messages to be printed.
 *
 * @return pdPASS if success, pdFAIL otherwise.
 */
BaseType_t xLoggingTaskInitialize( uint16_t usStackSize,
                                   UBaseType_t uxPriority,
                                   UBaseType_t uxQueueLength );

#endif /* #ifndef LOGGING_H */
