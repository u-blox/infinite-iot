#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_data.h"
#include "eh_config.h" // for APN, USERNAME and PASSWORD
#include "eh_utilities.h" // for ARRAY_SIZE
#include "act_cellular.h"
#include "act_modem.h"

using namespace utest::v1;

// These are tests for the eh_modem module.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define TRACE_GROUP "MODEM"

// The server address for test_send_reports()
#define SERVER_ADDRESS IOT_SERVER_IP_ADDRESS

// The server port for test_send_reports()
#define SERVER_PORT IOT_SERVER_PORT

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex gMtx;

// Storage for data contents
static DataContents gContents;

// ----------------------------------------------------------------
// PRIVATE FUNCTIONS
// ----------------------------------------------------------------

#ifdef MBED_CONF_MBED_TRACE_ENABLE
// Locks for debug prints
static void lock()
{
    gMtx.lock();
}

static void unlock()
{
    gMtx.unlock();
}
#endif

// Create a data item with valid contents
static Data *pCreateDataItem(DataContents *pContents, DataType type, char flags, Action *pAction)
{
    // For most things just fill the data contents with 0xFF
    // as that shows whether negative (or not) numbers are represented
    // properly
    memset (pContents, 0xFF, sizeof (*pContents));
    if (type == DATA_TYPE_BLE) {
        // For BLE, the name has to be a valid string or it won't print properly
        strcpy(gContents.ble.name, "BLE-THING");
    } else if (type == DATA_TYPE_LOG) {
        // Need a valid number of items
        pContents->log.numItems = ARRAY_SIZE(gContents.log.log);
    } else if (type == DATA_TYPE_WAKE_UP_REASON) {
        // Wake-up reason needs to be a valid one
        pContents->wakeUpReason.reason = WAKE_UP_ACCELERATION;
    }
    return pDataAlloc(pAction, type, flags, pContents);
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test initialisation
void test_init() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK)
    modemDeinit();

    // Now do it again: should be quicker this timer around as the modem
    // module will remember what sort of modem is attached
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK)
    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test getting the IMEI
void test_get_imei() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    char buf[MODEM_IMEI_LENGTH];

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Ask for the IMEI before the modem is initialised: should fail
    TEST_ASSERT(modemGetImei(buf) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Ask for the IMEI again
    TEST_ASSERT(modemGetImei(buf) == ACTION_DRIVER_OK);
    tr_debug("IMEI: %s.", buf);
    TEST_ASSERT(strlen(buf) == 15);

    // Ask with the parameter NULL
    TEST_ASSERT(modemGetImei(NULL) == ACTION_DRIVER_OK);

    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test getting the time
void test_get_time() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    struct tm *localTime;
    char timeString[25];
    time_t timeUTC;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Ask for a connection and the time before the modem is initialised
    TEST_ASSERT(modemConnect() == ACTION_DRIVER_ERROR_NOT_INITIALISED);
    TEST_ASSERT(modemGetTime(&timeUTC) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Ask to connect
    tr_debug("Connecting...\n");
    TEST_ASSERT(modemConnect() == ACTION_DRIVER_OK);
    // Ask for the time
    tr_debug("Getting the time...\n");
    TEST_ASSERT(modemGetTime(&timeUTC) == ACTION_DRIVER_OK);
    localTime = localtime(&timeUTC);
    if (localTime) {
        if (strftime(timeString, sizeof(timeString), "%a %b %d %H:%M:%S %Y", localTime) > 0) {
            tr_debug("NTP timestamp is %s.\n", timeString);
        }
    }
    // Do a bounds check of sorts
    TEST_ASSERT(timeUTC > 1529687605);
    // Ask again with NULL parameter
    TEST_ASSERT(modemGetTime(NULL) == ACTION_DRIVER_OK);

    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test getting the cellular receive signal strengths
void test_get_rx_signal_strengths() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    int rsrpDbm;
    int rssiDbm;
    int rsrqDb;
    int snrDb;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Ask for them before the modem is initialised
    TEST_ASSERT(getCellularSignalRx(&rsrpDbm, &rssiDbm, &rsrqDb, &snrDb) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Ask for the signal strengths
    tr_debug("Getting signal strengths...\n");
    TEST_ASSERT(getCellularSignalRx(&rsrpDbm, &rssiDbm, &rsrqDb, &snrDb) == ACTION_DRIVER_OK);
    tr_debug("RSRP: %d dBm, RSSI: %d dBm, RSRQ; %d dB, SNR: %d dB.",
             rsrpDbm, rssiDbm, rsrqDb, snrDb);

    // Ask again with NULL parameters
    TEST_ASSERT(getCellularSignalRx(&rsrpDbm, &rssiDbm, &rsrqDb, NULL) == ACTION_DRIVER_OK);
    TEST_ASSERT(getCellularSignalRx(&rsrpDbm, &rssiDbm, NULL, &snrDb) == ACTION_DRIVER_OK);
    TEST_ASSERT(getCellularSignalRx(&rsrpDbm, NULL, &rsrqDb, &snrDb) == ACTION_DRIVER_OK);
    TEST_ASSERT(getCellularSignalRx(NULL, &rssiDbm, &rsrqDb, &snrDb) == ACTION_DRIVER_OK);

    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test getting the cellular transmit signal strength
void test_get_tx_signal_strength() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    int powerDbm;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Ask for them before the modem is initialised
    TEST_ASSERT(getCellularSignalTx(&powerDbm) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Ask for the tx signal strengths
    tr_debug("Getting TX signal power...\n");
    if (modemIsN2()) {
        TEST_ASSERT(getCellularSignalTx(&powerDbm) == ACTION_DRIVER_OK);
        tr_debug("TX Power: %d dBm.", powerDbm);
    } else {
        TEST_ASSERT(modemIsR2());
        TEST_ASSERT(getCellularSignalTx(&powerDbm) == ACTION_DRIVER_ERROR_NO_DATA);
    }

    // Ask again with NULL parameter
    if (modemIsN2()) {
        TEST_ASSERT(getCellularSignalTx(NULL) == ACTION_DRIVER_OK);
    } else {
        TEST_ASSERT(modemIsR2());
        TEST_ASSERT(getCellularSignalTx(NULL) == ACTION_DRIVER_ERROR_NO_DATA);
    }

    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test getting the cellular channel parameters
void test_get_channel() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    unsigned int cellId;
    unsigned int earfcn;
    unsigned char ecl;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Ask for them before the modem is initialised
    TEST_ASSERT(getCellularChannel(&cellId, &earfcn, &ecl) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Ask for the channel parameters
    tr_debug("Getting channel parameters...\n");
    if (modemIsN2()) {
        TEST_ASSERT(getCellularChannel(&cellId, &earfcn, &ecl) == ACTION_DRIVER_OK);
        tr_debug("Cell ID: %u, EARFCN: %u, ECL: %u.", cellId, earfcn, ecl);
    } else {
        TEST_ASSERT(modemIsR2());
        TEST_ASSERT(getCellularChannel(&cellId, &earfcn, &ecl) == ACTION_DRIVER_ERROR_NO_DATA);
    }

    // Ask again with NULLs
    if (modemIsN2()) {
        TEST_ASSERT(getCellularChannel(&cellId, &earfcn, NULL) == ACTION_DRIVER_OK);
        TEST_ASSERT(getCellularChannel(&cellId, NULL, &ecl) == ACTION_DRIVER_OK);
        TEST_ASSERT(getCellularChannel(NULL, &earfcn, &ecl) == ACTION_DRIVER_OK);
    } else {
        TEST_ASSERT(modemIsR2());
        TEST_ASSERT(getCellularChannel(&cellId, &earfcn, NULL) == ACTION_DRIVER_ERROR_NO_DATA);
        TEST_ASSERT(getCellularChannel(&cellId, NULL, &ecl) == ACTION_DRIVER_ERROR_NO_DATA);
        TEST_ASSERT(getCellularChannel(NULL, &earfcn, &ecl) == ACTION_DRIVER_ERROR_NO_DATA);
    }

    modemDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test sending reports:
// NOTE: for this to pass you must run the Python script
// tec_eh_modem_test_server.py on a server that is visible
// to the modem on the public internet.  Make sure that
// the IP address and port number of the machine you use
// is set up correctly above.
void test_send_reports() {
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;
    Action action;
    int x = 0;
    int dataType;
    char flags;
    bool fullUp = false;
    char *pBuf;
    char idString[MODEM_IMEI_LENGTH];

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset, %d available.", (int) statsHeapBefore.current_size,
            (int) (statsHeapBefore.reserved_size - statsHeapBefore.current_size));

    // Malloc a buffer
    pBuf = (char *) malloc(1500);
    TEST_ASSERT (pBuf != NULL);

    // Create a data queue, every other thing requiring an ack
    // random things being "send now"
    tr_debug("Creating data items...\n");
    action.energyCostNWH = 0xFFFFFFFF;
    for (x = 0; !fullUp && (x < 50); x++) {
        dataType = (rand() % (MAX_NUM_DATA_TYPES - 1)) + 1; // -/+1 to avoid the NULL data type
        flags = ((x & 0x01) == 0) ? DATA_FLAG_REQUIRES_ACK : 0;
        flags |= ((rand() & 0x01) == 0) ? DATA_FLAG_SEND_NOW : 0;
        mbed_stats_heap_get(&statsHeapAfter);
        if (((statsHeapAfter.reserved_size - statsHeapAfter.current_size) < MODEM_HEAP_REQUIRED_BYTES) ||
            (pCreateDataItem(&gContents, (DataType) dataType, flags, &action) == NULL)) {
            fullUp = true;
        }
    }
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap remain after creating %d data item(s).\n",
             (int) (statsHeapAfter.reserved_size - statsHeapAfter.current_size), x);

    // Ask to send a report before the modem is initialised
    TEST_ASSERT(modemSendReports(SERVER_ADDRESS, SERVER_PORT,
                                 idString) == ACTION_DRIVER_ERROR_NOT_INITIALISED);

    // Initialise the modem
    TEST_ASSERT(modemInit(SIM_PIN, APN, USERNAME, PASSWORD) == ACTION_DRIVER_OK);

    // Get the IMEI to use as the ID
    TEST_ASSERT(modemGetImei(idString) == ACTION_DRIVER_OK);

    // Ask to connect and send the data
    tr_debug("Connecting...\n");
    TEST_ASSERT(modemConnect() == ACTION_DRIVER_OK);
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap remain after connecting to cellular.\n",
             (int) (statsHeapAfter.reserved_size - statsHeapAfter.current_size));
    tr_debug("Sending reports...\n");
    TEST_ASSERT(modemSendReports(SERVER_ADDRESS, SERVER_PORT, idString) == ACTION_DRIVER_OK);
    tr_debug("%d byte(s) of heap remain after sending.\n",
             (int) (statsHeapAfter.reserved_size - statsHeapAfter.current_size));

    // If we've sent everything and everything has been acknowledged
    // then a call to sort the data should return a NULL pointer
    TEST_ASSERT(pDataSort() == NULL);

    modemDeinit();

    free(pBuf);

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a timeout
    GREENTEA_SETUP(240, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Initialisation", test_init),
    Case("Get IMEI", test_get_imei),
    Case("Get time", test_get_time),
    Case("Get RX signal strengths", test_get_rx_signal_strengths),
    Case("Get TX signal strength", test_get_tx_signal_strength),
    Case("Get channel", test_get_channel),
    Case("Send reports", test_send_reports)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

int main()
{

#ifdef MBED_CONF_MBED_TRACE_ENABLE
    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(lock);
    mbed_trace_mutex_release_function_set(unlock);
#endif

    // Run tests
    return !Harness::run(specification);
}

// End Of File
