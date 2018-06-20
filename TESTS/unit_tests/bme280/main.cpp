#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "mbed_trace.h"
#include "mbed.h"
#include "eh_config.h"
#include "eh_i2c.h"
#include "act_bme280.h"
#include "act_temperature_humidity_pressure.h"
#define TRACE_GROUP "PROCESSOR"

using namespace utest::v1;

// These are tests for the act_bme280 temperature,
// humidity and pressure sensor driver.
// To run them, before you do "mbed test", you need
// to (once) do "mbedls --m 0004:UBLOX_EVK_NINA_B1" to
// set up the right target name, otherwise Mbed thinks
// you have an LPC2368 attached.
//
// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

#define BME280_ADDRESS BME280_DEFAULT_ADDRESS
#define I2C_DATA       PIN_I2C_SDA
#define I2C_CLOCK      PIN_I2C_SCL

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex gMtx;

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

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test of initialisation
void test_init() {
    int x = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    tr_debug("Initialising BME280...");
    x = bme280Init(BME280_ADDRESS);
    tr_debug("Result of initialising BME280 was %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    bme280Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test humidity reading
void test_humidity() {
    int x = 0;
    unsigned char percentage = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out as tr_debug seems to allocate from the heap when first called.\n");

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to take a reading before initialisation - should fail
    TEST_ASSERT(getHumidity(&percentage) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising BME280...");
    TEST_ASSERT(bme280Init(BME280_ADDRESS) == ACTION_DRIVER_OK);

    // Get a humidity reading
    tr_debug("Reading humidity...");
    x = getHumidity(&percentage);
    tr_debug("Result of reading humidity is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Humidity is %d%%.", percentage);
    TEST_ASSERT(percentage <= 100);

    // Repeat with null parameter
    TEST_ASSERT(getHumidity(NULL) == ACTION_DRIVER_OK);

    bme280Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test atmospheric pressure reading
void test_pressure() {
    int x = 0;
    unsigned int pascalX100 = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to take a reading before initialisation - should fail
    TEST_ASSERT(getPressure(&pascalX100) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising BME280...");
    TEST_ASSERT(bme280Init(BME280_ADDRESS) == ACTION_DRIVER_OK);

    // Get a pressure reading
    tr_debug("Reading pressure...");
    x = getPressure(&pascalX100);
    tr_debug("Result of reading pressure is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Pressure is %.2f Pascals.", ((float) pascalX100) / 100);

    // Repeat with null parameter
    TEST_ASSERT(getPressure(NULL) == ACTION_DRIVER_OK);

    bme280Deinit();

    // Shut down I2C
    i2cDeinit();

    // Capture the heap stats once more
    mbed_stats_heap_get(&statsHeapAfter);
    tr_debug("%d byte(s) of heap used at the end.", (int) statsHeapAfter.current_size);

    // The heap used should be the same as at the start
    TEST_ASSERT(statsHeapBefore.current_size == statsHeapAfter.current_size);
}

// Test temperature reading
void test_temperature() {
    int x = 0;
    signed int cX100 = 0;
    mbed_stats_heap_t statsHeapBefore;
    mbed_stats_heap_t statsHeapAfter;

    tr_debug("Print something out with a float (%f) in it as tr_debug and floats allocate from the heap when first called.\n", 1.0);

    // Capture the heap stats before we start
    mbed_stats_heap_get(&statsHeapBefore);
    tr_debug("%d byte(s) of heap used at the outset.", (int) statsHeapBefore.current_size);

    // Instantiate I2C
    i2cInit(I2C_DATA, I2C_CLOCK);

    // Try to take a reading before initialisation - should fail
    TEST_ASSERT(getTemperature(&cX100) == ACTION_DRIVER_ERROR_NOT_INITIALISED)

    tr_debug("Initialising BME280...");
    TEST_ASSERT(bme280Init(BME280_ADDRESS) == ACTION_DRIVER_OK);

    // Get a temperature reading
    tr_debug("Reading temperatre...");
    x = getTemperature(&cX100);
    tr_debug("Result of reading temperature is %d.", x);
    TEST_ASSERT(x == ACTION_DRIVER_OK);
    tr_debug("Temperature is %.2f C.", ((float) cX100) / 100);

    // Repeat with null parameter
    TEST_ASSERT(getTemperature(&cX100) == ACTION_DRIVER_OK);

    bme280Deinit();

    // Shut down I2C
    i2cDeinit();

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
    GREENTEA_SETUP(60, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Initialisation", test_init),
    Case("Get humidity", test_humidity),
    Case("Get pressure", test_pressure),
    Case("Get temperature", test_temperature)
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
