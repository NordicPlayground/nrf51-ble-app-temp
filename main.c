/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the license.txt file.
 */
/*
 * See README.md for a description of the application. 
 */ 

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_util.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "boards.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "nrf_soc.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT 0                                           /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#if (NRF_SD_BLE_API_VERSION == 3)
#define NRF_BLE_MAX_MTU_SIZE            GATT_MTU_SIZE_DEFAULT                       /**< MTU size used in the softdevice enabling and to reply to a BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST event. */
#endif

#define ADV_INTERVAL_IN_MS              1200
#define VBAT_MAX_IN_MV                  3300

#define ADVERTISING_LED_PIN_NO          LED_1                                       /**< Is on when device is advertising. */

#define CENTRAL_LINK_COUNT              0                                           /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                           /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define ADV_INTERVAL                    MSEC_TO_UNITS(ADV_INTERVAL_IN_MS, UNIT_0_625_MS) /**< The advertising interval (in units of 0.625 ms. */
#define ADV_TIMEOUT_IN_SECONDS          0                                           /**< The advertising timeout (in units of seconds). */

#define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            1                                           /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                           /**< Size of timer operation queues. */
#define ADVDATA_UPDATE_INTERVAL         APP_TIMER_TICKS(ADV_INTERVAL_IN_MS, APP_TIMER_PRESCALER)

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */


APP_TIMER_DEF(m_advdata_update_timer);

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze 
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
static void leds_init(void)
{
    nrf_gpio_cfg_output(ADVERTISING_LED_PIN_NO);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function shall be used to setup all the necessary GAP (Generic Access Profile) 
 *          parameters of the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_sec_mode_t sec_mode;
    
    char name_buffer[9];
    sprintf(name_buffer, "%08X", (unsigned int)NRF_FICR->DEVICEID[0]);
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)name_buffer, 
                                          strlen(name_buffer));
    APP_ERROR_CHECK(err_code);  
}

uint8_t battery_level_get(void)
{
#ifdef ADC_PRESENT
    // Configure ADC
    NRF_ADC->CONFIG     = (ADC_CONFIG_RES_8bit                        << ADC_CONFIG_RES_Pos)     |
                          (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos)  |
                          (ADC_CONFIG_REFSEL_VBG                      << ADC_CONFIG_REFSEL_Pos)  |
                          (ADC_CONFIG_PSEL_Disabled                   << ADC_CONFIG_PSEL_Pos)    |
                          (ADC_CONFIG_EXTREFSEL_None                  << ADC_CONFIG_EXTREFSEL_Pos);
    NRF_ADC->EVENTS_END = 0;
    NRF_ADC->ENABLE     = ADC_ENABLE_ENABLE_Enabled;

    NRF_ADC->EVENTS_END  = 0;    // Stop any running conversions.
    NRF_ADC->TASKS_START = 1;
    
    while (!NRF_ADC->EVENTS_END)
    {
    }
    
    uint16_t vbg_in_mv = 1200;
    uint8_t adc_max = 255;
    uint16_t vbat_current_in_mv = (NRF_ADC->RESULT * 3 * vbg_in_mv) / adc_max;
    
    NRF_ADC->EVENTS_END     = 0;
    NRF_ADC->TASKS_STOP     = 1;
    
    return (uint8_t) ((vbat_current_in_mv * 100) / VBAT_MAX_IN_MV);
#else // SAADC_PRESENT
	// SAADC not supported yet, just say the battery level is 100 for now
	return 100;
#endif
}

uint32_t temperature_data_get(void)
{
    int32_t temp;
    uint32_t err_code;
    
    err_code = sd_temp_get(&temp);
    APP_ERROR_CHECK(err_code);
    
    temp = (temp / 4) * 100;
    
    int8_t exponent = -2;
    return ((exponent & 0xFF) << 24) | (temp & 0x00FFFFFF);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advdata_update(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    
    ble_advdata_service_data_t service_data[2];
    
    uint8_t battery_data = battery_level_get();
    uint32_t temperature_data = temperature_data_get();
    
    service_data[0].service_uuid = BLE_UUID_BATTERY_SERVICE;
    service_data[0].data.size    = sizeof(battery_data);
    service_data[0].data.p_data  = &battery_data;

    service_data[1].service_uuid = BLE_UUID_HEALTH_THERMOMETER_SERVICE;
    service_data[1].data.size    = sizeof(temperature_data);
    service_data[1].data.p_data  = (uint8_t *) &temperature_data;

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type            = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance   = false;
    advdata.flags                = flags;
    advdata.service_data_count   = 2;
    advdata.p_service_data_array = service_data;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);
}

void advdata_update_timer_timeout_handler(void * p_context)
{
    advdata_update();
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    // Initialize timer module, making it use the scheduler
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
    
    uint32_t err_code = app_timer_create(&m_advdata_update_timer, APP_TIMER_MODE_REPEATED, advdata_update_timer_timeout_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting timers.
*/
static void timers_start(void)
{
    uint32_t err_code = app_timer_start(m_advdata_update_timer, ADVDATA_UPDATE_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t             err_code;
    ble_gap_adv_params_t adv_params;
    
    // Start advertising
    memset(&adv_params, 0, sizeof(adv_params));
    
    adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
    adv_params.p_peer_addr = NULL;
    adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    adv_params.interval    = ADV_INTERVAL;
    adv_params.timeout     = ADV_TIMEOUT_IN_SECONDS;

    err_code = sd_ble_gap_adv_start(&adv_params);
    APP_ERROR_CHECK(err_code);
    nrf_gpio_pin_clear(ADVERTISING_LED_PIN_NO);
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code = NRF_SUCCESS;
    
    switch (p_ble_evt->header.evt_id)
    {
        default:
            break;
    }

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    on_ble_evt(p_ble_evt);
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
#if (NRF_SD_BLE_API_VERSION == 3)
    ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    // Initialize
    leds_init();
    ble_stack_init();
    gap_params_init();
    timers_init();
    advdata_update();
    
    // Start execution
    timers_start();
    advertising_start();
    
    // Enter main loop
    for (;;)
    {
        power_manage();
    }
}

/** 
 * @}
 */
