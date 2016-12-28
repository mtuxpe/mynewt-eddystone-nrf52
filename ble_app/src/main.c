#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "bsp/bsp.h"
#include "os/os.h"
#include "sysinit/sysinit.h"

#include "hal/hal_gpio.h"

// BLE
#include "nimble/ble.h"
#include "host/ble_eddystone.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"


#define ADV_INTERVAL 300 // advertising interval miliseconds

// uncomment line below for eddystone UID
#define BLE_ADVERTISE_UID
// uncomment line below for eddystone URL
//#define BLE_ADVERTISE_URL
//#define BLE_ADVERTISE_TLM (not yet)


/** ble task settings. */
#define BLEPRPH_TASK_PRIO           1
#define BLEPRPH_STACK_SIZE          (OS_STACK_ALIGN(336))

const char url_mynewt [] = "mynewt.apache";

struct os_eventq bleprph_evq;
struct os_task bleprph_task;
bssnz_t os_stack_t bleprph_stack[BLEPRPH_STACK_SIZE];

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */

#ifdef BLE_ADVERTISE_UID
static void ble_eddystone_adv(void)  
{
    char uid[16] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff };

    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Indicate that the flags field should be included; specify a value of 0
     * to instruct the stack to fill the value in for us.
     */
    fields.flags_is_present = 1;
    fields.flags = 0;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this one automatically as well.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_eddystone_set_adv_data_uid (&fields,(void*)&uid );
    if ( rc != 0 )
    {
//        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
//        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_ADDR_TYPE_PUBLIC, 0, NULL, ADV_INTERVAL,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
//        BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

#endif

#ifdef BLE_ADVERTISE_URL

static void ble_eddystone_adv(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Indicate that the flags field should be included; specify a value of 0
     * to instruct the stack to fill the value in for us.
     */
    fields.flags_is_present = 1;
    fields.flags = 0;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this one automatically as well.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    // fields.tx_pwr_lvl_is_present = 1;
    // fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_eddystone_set_adv_data_url(&fields,
        BLE_EDDYSTONE_URL_SCHEME_HTTP,(char *) &url_mynewt,
        strlen(url_mynewt), BLE_EDDYSTONE_URL_SUFFIX_ORG);

    if (rc != 0) {
//        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
//        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_ADDR_TYPE_PUBLIC, 0, NULL, ADV_INTERVAL,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
//        BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

#endif

static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) 
    {
       case BLE_GAP_EVENT_ADV_COMPLETE:
           hal_gpio_toggle(LED_BLINK_PIN);
           ble_eddystone_adv();
           break;
       default:
           break;
    }
    return 0;
}


static void bleprph_on_sync(void)
{
    /* Begin advertising. */
    ble_eddystone_adv();
    hal_gpio_write(LED_BLINK_PIN,0);
}

/**
 * Event loop for the main ble task.
 */
void bleprph_task_handler(void *unused)
{
    while (1) {
        os_eventq_run(&bleprph_evq);
    }
}

int main(void)
{
    memcpy(g_dev_addr,(uint8_t[6]) { 0x0a,0x0a,0x0a,0x0a,0x0a,0x0a},6);
    /* Initialize OS */
    sysinit();

    /* Set LED1 to LED4 GPIO out */

    for (int i=0, led_pin=LED_BLINK_PIN; i<4 ; i++)
	    hal_gpio_init_out(led_pin++, 1);

    // Turn off all LEDs

    for ( int i=0, led_pin=LED_BLINK_PIN; i<4 ; i++)
            hal_gpio_write(led_pin++, 1);


    /* Initialize eventq */
    os_eventq_init(&bleprph_evq);

    /* Create the bleprph task.  All application logic and NimBLE host
     * operations are performed in this task.
     */
    os_task_init(&bleprph_task, "bleprph", bleprph_task_handler,
                 NULL, BLEPRPH_TASK_PRIO, OS_WAIT_FOREVER,
                 bleprph_stack, BLEPRPH_STACK_SIZE);


    ble_hs_cfg.sync_cb = bleprph_on_sync;

    os_eventq_dflt_set(&bleprph_evq);
    
    /* Start the OS */

    os_start();

    /* os_start should never return. If it does, this should be an error */
    assert(0);
}

