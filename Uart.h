#ifndef __Uart_H
#define __Uart_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_uart.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf.h"
#include "bsp.h"

void uart_error_handle(app_uart_evt_t * p_event);
void uart_init(void);
void Send_Data_uint24 (uint32_t data);
void Send_Data_uint32 (uint32_t data);
void Send_Header(uint16_t head1, uint16_t head2);
#endif