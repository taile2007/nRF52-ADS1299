/**
 * Copyright (c) 2014 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "softdevice_handler.h"
#include "nrf_ble_gatt.h"
#include "app_timer.h"
#include "app_button.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "nrf_drv_spi.h"
#include "nrf_delay.h"
#include "boards.h"
#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define CONN_CFG_TAG                    1                                           /**< A tag that refers to the BLE stack configuration we set with @ref sd_ble_cfg_set. Default tag is @ref BLE_CONN_CFG_TAG_DEFAULT. */

#define APP_FEATURE_NOT_SUPPORTED       BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */

#define DEVICE_NAME                     "ECG_01_test"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      180                                         /**< The advertising timeout (in units of seconds). */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(10, UNIT_1_25_MS)             /**< 10*1000/1250 = 8ms*/
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< 40*1000/1250 = 32ms */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256                                         /**< UART RX buffer size. */

static ble_nus_t                        m_nus;                                      /**< Structure to identify the Nordic UART Service. */
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current connection. */

static nrf_ble_gatt_t                   m_gatt;                                     /**< GATT module instance. */
static ble_uuid_t                       m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}};  /**< Universally unique service identifier. */
static uint16_t                         m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;  /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */

#define DEBUG 1
#define SPI_INSTANCE  0 /**< SPI instance index. */
#define DRDY 	16 // pin 24
#define RESET	15 // pin 23 // changed to pin 15 in PCB design
#define START_01	22 // pin 22
#define PWDN	20	// pin 20 
#define CS		11	// pin 29
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);
static volatile bool spi_xfer_done;
#define len_data_send 16
static uint8_t 			m_tx_buf[4]; /**< Tx bufffer contains address of Slave register */
static uint8_t			m_rx_buf[len_data_send]; /**< RX buffer get data from reading in Slave register */
uint8_t spiDataTx[1];
uint8_t dataToSend[3];
volatile static bool spi_tx_done = false;
volatile bool ads1299dataReceived = false;
bool isConnected = false;
/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_nus    Nordic UART Service structure.
 * @param[in] p_data   Data to be send to UART module.
 * @param[in] length   Length of the data.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length)
{
    uint32_t err_code;

    NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.\r\n");
    NRF_LOG_HEXDUMP_DEBUG(p_data, length);

    for (uint32_t i = 0; i < length; i++)
    {
        do
        {
            err_code = app_uart_put(p_data[i]);
            if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
            {
                NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. \r\n", err_code);
                APP_ERROR_CHECK(err_code);
            }
        } while (err_code == NRF_ERROR_BUSY);
    }
    if (p_data[length-1] == '\r')
    {
        while (app_uart_put('\n') == NRF_ERROR_BUSY);
    }

}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t       err_code;
    ble_nus_init_t nus_init;

    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for the application's SoftDevice event handler.
 *
 * @param[in] p_ble_evt SoftDevice event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            NRF_LOG_INFO("Connected\r\n");
            break; // BLE_GAP_EVT_CONNECTED

        case BLE_GAP_EVT_DISCONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            NRF_LOG_INFO("Disconnected\r\n");
            break; // BLE_GAP_EVT_DISCONNECTED

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GAP_EVT_SEC_PARAMS_REQUEST

         case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
        {
            ble_gap_data_length_params_t dl_params;

            // Clearing the struct will effectivly set members to @ref BLE_GAP_DATA_LENGTH_AUTO
            memset(&dl_params, 0, sizeof(ble_gap_data_length_params_t));
            err_code = sd_ble_gap_data_length_update(p_ble_evt->evt.gap_evt.conn_handle, &dl_params, NULL);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_SYS_ATTR_MISSING

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTC_EVT_TIMEOUT

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_TIMEOUT

        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gattc_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_EVT_USER_MEM_REQUEST

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t  req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;

            req = p_ble_evt->evt.gatts_evt.params.authorize_request;

            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a SoftDevice event to all modules with a SoftDevice
 *        event handler.
 *
 * @details This function is called from the SoftDevice event interrupt handler after a
 *          SoftDevice event has been received.
 *
 * @param[in] p_ble_evt  SoftDevice event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    ble_conn_params_on_ble_evt(p_ble_evt);
    nrf_ble_gatt_on_ble_evt(&m_gatt, p_ble_evt);
    ble_nus_on_ble_evt(&m_nus, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    bsp_btn_ble_on_ble_evt(p_ble_evt);

}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize SoftDevice.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = softdevice_app_ram_start_get(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Overwrite some of the default configurations for the BLE stack.
    ble_cfg_t ble_cfg;

    // Configure the maximum number of connections.
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = BLE_GAP_ROLE_COUNT_PERIPH_DEFAULT;
    ble_cfg.gap_cfg.role_count_cfg.central_role_count = 0;
    ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = 0;
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Configure the maximum ATT MTU.
    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag                 = CONN_CFG_TAG;
    ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = NRF_BLE_GATT_MAX_MTU_SIZE;
    err_code = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Configure the maximum event length.
    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag                     = CONN_CFG_TAG;
    ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 320;
    ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count   = BLE_GAP_CONN_COUNT_DEFAULT;
    err_code = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = softdevice_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Subscribe for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}
/*
**
*/

void spi_event_handler(nrf_drv_spi_evt_t const *p_event, void *p_context)
{
	spi_xfer_done = true;
	NRF_LOG_INFO("Transfer completed.\r\n");
	if (m_rx_buf[0] != 0)
	{
		NRF_LOG_INFO(" Received:%x\r\n", (uint32_t)m_rx_buf[0]);
		NRF_LOG_HEXDUMP_INFO(m_rx_buf, strlen((const char *)m_rx_buf));
	}
}
void mySPI_int(void) // check other parameters for initiate SPI function
{
	/**< set up RESET, SS, START is OUTPUT and DRDY is INPUT */
	nrf_gpio_cfg_output(START_01);
	nrf_gpio_cfg_output(PWDN);
	nrf_gpio_cfg_output(CS);
	nrf_gpio_cfg_output(RESET);
	nrf_gpio_cfg_input(DRDY, GPIO_PIN_CNF_PULL_Pullup);	
	
	nrf_gpio_pin_set(CS);
	nrf_gpio_pin_set(RESET);
	nrf_gpio_pin_clear(START_01);
	nrf_gpio_pin_set(PWDN);
	
	/**< SPI configuration */
	nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
	//spi_config.ss_pin = CS; 			//< pin 12
	spi_config.miso_pin = 12;//SPI_MISO_PIN; DOUT
	spi_config.mosi_pin = 13;//SPI_MOSI_PIN; DIN
	spi_config.sck_pin = 14;//SPI_SCK_PIN; 
	spi_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;
  spi_config.mode = NRF_DRV_SPI_MODE_1; 						// Clock polarity is 0 and clock phase (CHPA) = 1; MODE_0, so CPHA = 1.
  spi_config.frequency = NRF_DRV_SPI_FREQ_4M; 			// 2MB for datarate
	spi_config.irq_priority = APP_IRQ_PRIORITY_HIGH,
	spi_config.orc = 0x00; 														///< Over-run character. for 0xFF
	APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));
	NRF_LOG_INFO( "SPI example started.\r\n");	
}
//****** Power Up ADS  ******
void Reset_ADS(void)
{
	nrf_gpio_pin_set(PWDN);
	nrf_delay_us(100000);
	nrf_gpio_pin_clear(PWDN);
	nrf_delay_us(100000);
	nrf_gpio_pin_set(PWDN);
	nrf_delay_us(100000);
}
void PowerUp_ADS(void)
{
	//** Power Up and Digital Power Up **
	nrf_delay_us(1000000);					//Delay 1s for Power Up
	nrf_delay_us(400000); // // Delay t_por = 2^16 x t_CLK of ADS (1/2.048MHz)=0.032s => Delay 40ms CHECK!!!!
	nrf_gpio_pin_clear(RESET);
	nrf_delay_us(1); // // // Delay 2 x t_CLK of ADS = 2x(1/2.048MHz)=0.977us CHECK!!!!
	nrf_gpio_pin_set(RESET);
	nrf_delay_us(10); // // Delay 18 x t_CLK of ADS = 18x(1/2.048MHz)=8.789us to start using the Device CHECK!!!!
	nrf_delay_us(1000000);					//Delay 1s for Power-on Reset
}

//****** RREG Command ADS  ******
// How to read data back from slave???????
uint8_t RREG_ADS(uint8_t address)
{
		dataToSend[0] = address | 0x20;				//Opcode 000n nnnn =0 : write n channels
		dataToSend[1] = 0x00;								// specify how many registers used
  
	  nrf_gpio_pin_clear(CS); // enable SS pin for ADS
	
		if(DEBUG){NRF_LOG_INFO("Read from DR register of ADS\r\n");NRF_LOG_FLUSH();}
	  // Reset rx buffer and transfer done flag
	  spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
    APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, dataToSend, 2, m_rx_buf, 1));
	  while (!spi_xfer_done){__WFE();}
	  NRF_LOG_FLUSH();
		
		// send dummy bytes		
		if(DEBUG){NRF_LOG_INFO("Send dummy bytes_for RREG_ADS\r\n");NRF_LOG_FLUSH();}
	  spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
	  APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, 0x00, 1, m_rx_buf,1));  
	  while (!spi_xfer_done){__WFE();}
	  NRF_LOG_FLUSH();  
		nrf_delay_us(15);
		nrf_gpio_pin_set(CS); // disable SS pin for ADS
		return m_rx_buf[0];	
}

//****** Regcognize ADS1299  ******
uint8_t ADS_ID; uint16_t reg_count; uint8_t test[4];
uint8_t i;
void Recognize_ADS(void)
{	
  ADS_ID = RREG_ADS(0x00);
/*
	for (i = 0; i < 4; i++)
	{
		test[i] = *(ADS_ID +i);
		if(DEBUG){NRF_LOG_INFO("ADS_ID[%d]: %X\r\n",(uint32_t)i, (uint32_t)test[i]); NRF_LOG_FLUSH();}
	}
*/
	if ((ADS_ID & 0x0f)==0x0e) // 0x0e is for 1299-8, 0x0c is for 1299-4
	{
		for (reg_count=0;reg_count<50;reg_count++)
		{
			bsp_board_led_invert(1);
      nrf_delay_ms(50);
		}
	//	bsp_board_led_invert(BSP_BOARD_LED_3);
	}
}

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
//****** GetData in RDATA mode  ******
void GetData_RDATA(void)
{
	uint8_t countbytes;
	uint8_t countbits;
	uint32_t data_raw[10][10];
	uint8_t temp_data[1];
	uint32_t dataChn[10];
	while(nrf_gpio_pin_read(DRDY) != 0); // wait for DRDY is low to indicate data ready for transmit
	m_tx_buf[0] = 0x12;
	nrf_gpio_pin_clear(CS);  // enable SS pin for ADS
	
	// Send Command RDATA: send RDATA opcode
	if(DEBUG){NRF_LOG_INFO("Send Command RDATA\r\n");NRF_LOG_FLUSH();}
	spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
	APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, m_tx_buf, 1, m_rx_buf, sizeof(m_rx_buf)));  
	while (!spi_xfer_done){__WFE();}
	NRF_LOG_FLUSH();
	
	for (countbytes = 1; countbytes < 10; countbytes++)
	{
		for (countbits = 3; countbits > 0; countbits--)
		{
			if(DEBUG){NRF_LOG_INFO("getdata_RDATA");NRF_LOG_FLUSH();}
		  spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
			APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, 0x00, 1, temp_data, 1));
			data_raw[countbytes][countbits] = temp_data[0];	
		  while (!spi_xfer_done){__WFE();}
		  NRF_LOG_FLUSH();	
					
			dataChn[countbytes] = dataChn[countbytes]|(data_raw[countbytes][countbits]<<((countbits-1)*8));
		}
		if (countbytes!=1)
			{
				if (dataChn[countbytes] > 0x007fffff)
				{
					dataChn[countbytes] = 0x00ffffff - dataChn[countbytes];
				}
				else 
				{
					dataChn[countbytes] = dataChn[countbytes] + 0x00800000;	
				}
			}
		
	}
	for (countbytes=1;countbytes<10;countbytes++)
	{
	//	Send_Data_uint24(dataChn[countbytes]);
	}
}


//****** Send other Command to ADS with appropriated time  ******
void SendADSCommand(uint8_t data, uint32_t delaytime)
{
	  nrf_gpio_pin_clear(CS); // enable SS pin for ADS
		spiDataTx[0] = data;
		if(DEBUG){NRF_LOG_INFO("SendADSCommand\r\n");NRF_LOG_FLUSH();}
	  spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
	  APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, spiDataTx, 1, m_rx_buf, 1));  
	  while (!spi_xfer_done){__WFE();}
	  NRF_LOG_FLUSH();

	  nrf_delay_us(delaytime); // Check TIME DELAY HERE????
	  nrf_gpio_pin_set(CS); // disable SS pin for ADS
}

/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
//****** GetData in RDATAC mode  ******
void GetData_RDATAC(void)
{
	uint8_t count = 0;
	SendADSCommand(0x10,2);
	dataToSend[0] = 0x00;
	uint8_t dummyData[1] = {0x00};
	if(DEBUG){NRF_LOG_INFO("getdata_RDATAC");NRF_LOG_FLUSH();}
	
	while(1)
	{
		while(nrf_gpio_pin_read(DRDY) != 0x00) // wait for DRDY is low to indicate data ready for transmit 
		nrf_gpio_pin_clear(CS);  // enable SS pin for ADS	
		spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
		APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, 0x00, 1, m_rx_buf,sizeof(m_rx_buf)));  
		while (!spi_xfer_done){__WFE();}
		NRF_LOG_FLUSH();
		nrf_gpio_pin_set(CS); // disable SS pin for ADS
		count = count +1;
		m_rx_buf[len_data_send-1] = count;
		ble_nus_string_send(&m_nus, m_rx_buf, len_data_send);
	}
	
}

//****** WREG Command ADS  ******
void WREG_ADS(uint8_t address, uint8_t data)
{
		dataToSend[0] = address | 0x40;
		dataToSend[1] = 0x00;
		dataToSend[2] = data;
		nrf_gpio_pin_clear(CS); //enable SS pin for ADS
	
		if(DEBUG){NRF_LOG_INFO("2.0");NRF_LOG_FLUSH();}
	  spi_xfer_done = false; memset(m_rx_buf,0,sizeof(m_rx_buf));
	  APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, dataToSend, 3, m_rx_buf, 1));  
	  while (!spi_xfer_done){__WFE();}
	  NRF_LOG_FLUSH();
	  nrf_delay_us(15); // Check TIME DELAY HERE????
	  nrf_gpio_pin_set(CS);	 // disable SS pin for ADS
}
/*
*
*/

/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, const nrf_ble_gatt_evt_t * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)\r\n", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x\r\n", p_gatt->att_mtu_desired_central, p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, 64);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist();
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') || (index >= (m_ble_nus_max_data_len)))
            {
                NRF_LOG_DEBUG("Ready to send data over BLE NUS\r\n");
                NRF_LOG_HEXDUMP_DEBUG(data_array, index);

                do
                {
                    err_code = ble_nus_string_send(&m_nus, data_array, index);
                    if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_BUSY) )
                    {
                        APP_ERROR_CHECK(err_code);
                    }
                } while (err_code == NRF_ERROR_BUSY);

                index = 0;
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
/**@snippet [Handling the data received over UART] */


/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
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
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud115200
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advdata_t          advdata;
    ble_advdata_t          scanrsp;
    ble_adv_modes_config_t options;
		int8_t tx_power_level = 0;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
		advdata.p_tx_power_level   = &tx_power_level;
			
		sd_ble_gap_tx_power_set(tx_power_level);
	
    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = m_adv_uuids;

    memset(&options, 0, sizeof(options));
    options.ble_adv_fast_enabled  = true;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for placing the application in low power state while waiting for events.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


/**@brief Application main function.
 */
int main(void)
{
    uint32_t err_code;
    bool     erase_bonds;

    // Initialize.
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    uart_init();
    log_init();

    buttons_leds_init(&erase_bonds);
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();

    printf("\r\nUART Start!\r\n");
    NRF_LOG_INFO("UART Start!\r\n");
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);

    // Enter main loop.
//    for (;;)
//    {
//        power_manage();
//    }
		mySPI_int();
		Reset_ADS();
		PowerUp_ADS(); 						// Power up ADS: Delay for system startup and reset ADS1299 chip
		SendADSCommand(0x11,2);		// Send Command SDATA: STOP RDATAC mode
		Recognize_ADS();					// Check the ADS device ID. If the ID is corrent, blink the LED.
		//uint8_t PGA[4] = {0x60, 0x50, 0x40, 0x30}; 
		WREG_ADS(0x01,0xD6);			//Config1: Daisy Chain Mode, Oscillator output disable, DT 250SPS. 1101 0110 250 normal > 1101 0101 500sps. 6 250. 5 500
		WREG_ADS(0x02,0xC0);			//Config2: Test Signals are driven Externally, Test Signal and Amplitude are Default
		WREG_ADS(0x03,0xE1);			//Config3: Unsing Internal Reference
		WREG_ADS(0x05,0x60);			//Channel 1-8: Normal Operation;PGA=24 ;SRB2 off;Normal electrode input 101 for Gain 12, and 110 for Gain 24
		WREG_ADS(0x06,0x60);			// Channel 2: 0110 0000 gain 24
		WREG_ADS(0x07,0x60);			//-
		WREG_ADS(0x08,0x60);			//-
//		WREG_ADS(0x09,0x30);			//-
//		WREG_ADS(0x0A,0x30);			//-
//		WREG_ADS(0x0B,0x30);			//-
//		WREG_ADS(0x0C,0x30);
		WREG_ADS(0x0D,0x50);
		WREG_ADS(0x0E,0x50);//-8
		
		//LOFF setup
		WREG_ADS(0x04,0x02);			// 04h LOFF: 000.0.00.10 Current source signal: 31Hz, 6nA > 1010 - 6uA 31Hz
		WREG_ADS(0x0F,0x00);			//Turn on current source for all P channel; turn on channel 1 to 4 // Connected all negative pin to SRB1 (0x20)
		WREG_ADS(0x10,0x00);			// Turn off current source for all N channel
		WREG_ADS(0x11,0x00);			// No flipping current sourcee			
		
		//* Square wave test signal 

//		WREG_ADS(0x02,0xD5);			//// 1101.0000 Internal signal test bit 4, test signal amplitude default. pulsed at default
//		WREG_ADS(0x05,0x05);
//		WREG_ADS(0x06,0x05);
//		WREG_ADS(0x07,0x04);			// For temperature sensors
//		WREG_ADS(0x08,0x05);
//		WREG_ADS(0x09,0x05);
//		WREG_ADS(0x0A,0x05);
//		WREG_ADS(0x0B,0x05);
//		WREG_ADS(0x0C,0x05);

		nrf_gpio_pin_set(START_01); //Enable START, then /DRDY toggle at f_CLK/8192 =>t=4ms
		nrf_delay_us(10);
		//GetData_RDATA();
		GetData_RDATAC();
}


/**
 * @}
 */
