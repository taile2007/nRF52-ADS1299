#include "Uart.h"

#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */

void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}
/**@snippet [UART Initialization] */
void uart_init(void)
{
    uint32_t                     err_code;
    const app_uart_comm_params_t comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud230400
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_error_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */
void Send_Data_uint24 (uint32_t data)
{
	uint8_t a=0,b=0,c=0;
	uint32_t err_code;
	a = (data/65536);
	b = (data%65536)/256;
	c = (data%65536)%256;
	err_code = app_uart_put(a);
	err_code = app_uart_put(b);
	err_code = app_uart_put(c);
}

void Send_Data_uint32 (uint32_t data)
{
	uint8_t a=0,b=0,c=0, d =0;
	uint32_t err_code;
	a=(data/16777216);
	b=(data%16777216)/65536;
	c=((data%16777216)%65536)/256;
	d=((data%16777216)%65536)%256;
	err_code = app_uart_put(a);
	err_code = app_uart_put(b);
	err_code = app_uart_put(c);
	err_code = app_uart_put(d);
}

void Send_Header(uint16_t head1, uint16_t head2)
{
	uint32_t err_code;
	err_code = app_uart_put(head1);
	err_code = app_uart_put(head2);
	//err_code = app_uart_put(head2);
	//err_code = app_uart_put(head1);
}
