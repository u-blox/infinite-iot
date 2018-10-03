/* The code here is borrowed from:
 *
 * https://os.mbed.com/teams/SiliconLabs/code/Si1133//#1e2dd643afa8
 *
 * All rights remain with the original author(s).
 */

#include <mbed.h>
#include <eh_utilities.h> // For ARRAY_SIZE and MTX_LOCK()/MTX_UNLOCK()
#include <eh_debug.h>
#include <eh_i2c.h>
#include <act_light.h>
#include <act_si1133.h>

/**************************************************************************
 * MANIFEST CONSTANTS
 *************************************************************************/

#define X_ORDER_MASK            0x0070
#define Y_ORDER_MASK            0x0007
#define SIGN_MASK               0x0080
#define GET_X_ORDER(m)          ( ((m) & X_ORDER_MASK) >> 4)
#define GET_Y_ORDER(m)          ( ((m) & Y_ORDER_MASK) )
#define GET_SIGN(m)             ( ((m) & SIGN_MASK) >> 7)

#define UV_INPUT_FRACTION       15
#define UV_OUTPUT_FRACTION      12
#define UV_NUMCOEFF             2

#define ADC_THRESHOLD           16000
#define INPUT_FRACTION_HIGH     7
#define INPUT_FRACTION_LOW      15
#define LUX_OUTPUT_FRACTION     12
#define NUMCOEFF_LOW            9
#define NUMCOEFF_HIGH           4

/**************************************************************************
 * TYPES
 *************************************************************************/

/** Structure to store the data measured by the Si1133.
 */
typedef struct {
    char    irqStatus;     /**< Interrupt status of the device    */
    int     ch0;           /**< Channel 0 measurement data        */
    int     ch1;           /**< Channel 1 measurement data        */
    int     ch2;           /**< Channel 2 measurement data        */
    int     ch3;           /**< Channel 3 measurement data        */
} Samples;

/** Structure to store the calculation coefficients.
 */
typedef struct {
    signed short   info;   /**< Info                              */
    unsigned short mag;    /**< Magnitude                         */
} Coeff;

/** Structure to store the coefficients used for Lux calculation.
 */
typedef struct {
    Coeff   coeffHigh[4];   /**< High amplitude coeffs */
    Coeff   coeffLow[9];    /**< Low amplitude coeffs  */
} LuxCoeff;

/**************************************************************************
 * LOCAL VARIABLES
 *************************************************************************/

/** Flag so that we know if we've been initialised.
 */
static bool gInitialised = false;

/** The I2C address of the SI1133.
 */
static char gI2cAddress = 0;

/** Mutex to protect the against multiple accessors.
 */
static Mutex gMtx;

/** Coefficients for lux calculation
 */
static const LuxCoeff lk = {
    {   {     0, 209 },           /**< coeffHigh[0]   */
        {  1665, 93  },           /**< coeffHigh[1]   */
        {  2064, 65  },           /**< coeffHigh[2]   */
        { -2671, 234 } },         /**< coeffHigh[3]   */
    {   {     0, 0   },           /**< coeffLow[0]    */
        {  1921, 29053 },         /**< coeffLow[1]    */
        { -1022, 36363 },         /**< coeffLow[2]    */
        {  2320, 20789 },         /**< coeffLow[3]    */
        {  -367, 57909 },         /**< coeffLow[4]    */
        { -1774, 38240 },         /**< coeffLow[5]    */
        {  -608, 46775 },         /**< coeffLow[6]    */
        { -1503, 51831 },         /**< coeffLow[7]    */
        { -1886, 58928 } }        /**< coeffLow[8]    */
};

/** Coefficients for UV index calculation
 */
static const Coeff uk[2] = {
    { 1281, 30902 },            /**< coeff[0]        */
    { -638, 46301 }             /**< coeff[1]        */
};

/** Initialisation address/value parameter pairs
 */
static const char initPairs[] = {0x01, 0x0f, /* PARAM_CH_LIST */
                                 0x02, 0x78, /* PARAM_ADCCONFIG0 */
                                 0x03, 0x71, /* PARAM_ADCSENS0 */
                                 0x04, 0x40, /* PARAM_ADCPOST0 */
                                 0x06, 0x4d, /* PARAM_ADCCONFIG1 */
                                 0x07, 0xe1, /* PARAM_ADCSENS1 */
                                 0x08, 0x40, /* PARAM_ADCPOST1 */
                                 0x0A, 0x41, /* PARAM_ADCCONFIG2 */
                                 0x0B, 0xe1, /* PARAM_ADCSENS2 */
                                 0x0C, 0x50, /* PARAM_ADCPOST2 */
                                 0x0E, 0x4d, /* PARAM_ADCCONFIG3 */
                                 0x0F, 0x87, /* PARAM_ADCSENS3 */
                                 0x10, 0x40  /* PARAM_ADCPOST3 */};

/**************************************************************************
 * STATIC FUNCTIONS
 *************************************************************************/

// Wait until the Si1133 is asleep.
static bool waitUntilSleep()
{
  bool success = false;;
  Timer timer;
  char data[2];

  // Loop until the Si1133 is known to be in its sleep state
  data[0] = 0x11; // REG_RESPONSE0;
  timer.reset();
  timer.start();
  while (!success && (timer.read_ms() < SI1133_WAIT_FOR_SLEEP_MS)) {
      if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
          success = ((data[1] & 0xE0) == 0x20); // RSP0_CHIPSTAT_MASK
      }
      Thread::wait(20); // Relax a little
  }
  timer.stop();

  return success;
}

// Wait until the response register has changed.
static bool waitUntilResponse(char currentValue)
{
  bool success = false;;
  Timer timer;
  char data[2];

  // Loop until the counter register changes value
  data[0] = 0x11; // REG_RESPONSE0;
  timer.reset();
  timer.start();
  while (!success && ((timer.read_ms() < SI1133_WAIT_FOR_RESPONSE_MS))) {
      if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) {
          success = ((data[1] & 0x1F) != currentValue); // RSP0_COUNTER_MASK
      }
      Thread::wait(20); // Relax a little
  }
  timer.stop();

  return success;
}

// Write a byte to an Si1133 parameter register.
static ActionDriver setParameter(char address, char value)
{
    ActionDriver result = ACTION_DRIVER_ERROR_CHIP_STATE;
    char responseStored;
    char data[3];

    // Wait for the device to go to sleep and read the
    // response counter
    data[0] = 0x11; // REG_RESPONSE0;
    if (waitUntilSleep()) {
        result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
        if (i2cSendReceive(gI2cAddress, data, 1, &responseStored, 1) == 1) {
            responseStored &= 0x1F; // RSP0_COUNTER_MASK

            data[0] = 0x0A; // REG_HOSTIN0;
            data[1] = value;
            data[2] = 0x80 + (address & 0x3F);

            // Write the parameter and wait for the response
            // counter to increment
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
            if (i2cSendReceive(gI2cAddress, data, 3, NULL, 0) == 0) {
                result = ACTION_DRIVER_ERROR_CHIP_STATE;
                if (waitUntilResponse(responseStored)) {
                    result = ACTION_DRIVER_OK;
                }
            }
        }
    }

    return result;
}

// Write a command to Si1133.
static ActionDriver sendCommand(char command)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    bool success = false;
    char responseStored;
    int count = 0;
    char data[2];

    // Read the response counter and wait for the chip
    // to go to sleep
    data[0] = 0x11; // REG_RESPONSE0;
    if (i2cSendReceive(gI2cAddress, data, 1, &responseStored, 1) == 1) {
        responseStored &= 0x1F; // RSP0_COUNTER_MASK

        // If the command is not to reset the command counter,
        // make sure that the counter value is consistent
        result = ACTION_DRIVER_ERROR_CHIP_STATE;
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
#pragma diag_suppress 1293  //  suppressing warning "assignment in condition" on ARMCC
#endif
        while (waitUntilSleep() && (command != 0) && (count < 5) &&
               (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) &&
               !(success = ((data[1] & 0x1F) == responseStored))) {
               responseStored = data[1] & 0x1F;  // RSP0_COUNTER_MASK
               count++;
        }

        // If all is good, send the command
        if (success) {
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
            data[0] = 0x0b; // REG_COMMAND
            data[1] = command;
            if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                result = ACTION_DRIVER_ERROR_CHIP_STATE;
                // Wait for a change in the response counter
                // if it's not being reset
                if ((command == 0) || waitUntilResponse(responseStored)) {
                    result = ACTION_DRIVER_OK;
                }
            }
        }
    }

    return result;
}

// Read the measurement results from the chip.
static ActionDriver readResults(Samples *pSamples)
{
    ActionDriver result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
    char data[14];

    data[0] = 0x12; // REG_IRQ_STATUS
    if (i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 13) == 13) {
        pSamples->irqStatus = data[1];

        pSamples->ch0 = data[2] << 16;
        pSamples->ch0 |= data[3] << 8;
        pSamples->ch0 |= data[4];
        if (pSamples->ch0 & 0x800000) {
            pSamples->ch0 |= 0xFF000000;
        }

        pSamples->ch1 = data[5] << 16;
        pSamples->ch1 |= data[6] << 8;
        pSamples->ch1 |= data[7];
        if (pSamples->ch1 & 0x800000) {
            pSamples->ch1 |= 0xFF000000;
        }

        pSamples->ch2 = data[8] << 16;
        pSamples->ch2 |= data[9] << 8;
        pSamples->ch2 |= data[10];
        if (pSamples->ch2 & 0x800000) {
            pSamples->ch2 |= 0xFF000000;
        }

        pSamples->ch3 = data[11] << 16;
        pSamples->ch3 |= data[12] << 8;
        pSamples->ch3 |= data[13];
        if (pSamples->ch3 & 0x800000) {
            pSamples->ch3 |= 0xFF000000;
        }

        result = ACTION_DRIVER_OK;
    }

    return result;
}

static int calculatePolynomialHelper(int input, char fraction, unsigned short mag, signed char shift)
{
    int value;

    value = ((input << fraction) / mag);
    if (shift < 0) {
        value >>= -shift;
    } else {
        value <<= shift;
    }

    return value;
}

static int calculatePolynomial(int x, int y, char inputFraction, char outputFraction, char numCoeff, const Coeff *pKp)
{
    unsigned char info;
    int xOrder;
    int yOrder;
    int counter;
    signed char sign;
    signed char shift;
    unsigned short mag;
    int output = 0;
    int x1;
    int x2;
    int y1;
    int y2;

    for (counter = 0; counter < numCoeff; counter++) {
        info = pKp->info;
        xOrder = GET_X_ORDER(info);
        yOrder = GET_Y_ORDER(info);

        shift = ((unsigned) pKp->info & 0xff00) >> 8;
        shift ^= 0x00ff;
        shift += 1;
        shift = -shift;

        mag = pKp->mag;

        sign = 1;
        if (GET_SIGN(info)) {
            sign = -1;
        }

        if ((xOrder == 0) && (yOrder == 0)) {
            output += sign * mag << outputFraction;
        } else {
            x1 = 1;
            x2 = 1;
            if (xOrder > 0) {
                x1 = calculatePolynomialHelper(x, inputFraction, mag, shift);
                x2 = 1;
                if (xOrder > 1) {
                    x2 = calculatePolynomialHelper(x, inputFraction, mag, shift);
                }
            }

            y1 = 1;
            y2 = 1;
            if (yOrder > 0) {
                y1 = calculatePolynomialHelper(y, inputFraction, mag, shift);
                y2 = 1;
                if (yOrder > 1) {
                    y2 = calculatePolynomialHelper(y, inputFraction, mag, shift);
                }
            }

            output += sign * x1 * x2 * y1 * y2;
        }

        pKp++;
    }

    if (output < 0) {
        output = -output;
    }

    return output;
}


// Derive a lux measurement from readings.
static int getLux(int visHigh, int visLow, int ir)
{
    int lux;

    if ((visHigh > ADC_THRESHOLD) || (ir > ADC_THRESHOLD)) {
        lux = calculatePolynomial(visHigh, ir, INPUT_FRACTION_HIGH,
                                  LUX_OUTPUT_FRACTION, NUMCOEFF_HIGH,
                                  &(lk.coeffHigh[0]));
    } else {
        lux = calculatePolynomial(visLow, ir, INPUT_FRACTION_LOW,
                                  LUX_OUTPUT_FRACTION, NUMCOEFF_LOW,
                                  &(lk.coeffLow[0]));
    }

    return lux;
}

// Derive the UV Index from readings.
static int getUvIndex(int uv)
{
    return calculatePolynomial(0, uv, UV_INPUT_FRACTION,
                               UV_OUTPUT_FRACTION, UV_NUMCOEFF,
                               uk);
}

/**************************************************************************
 * PUBLIC FUNCTIONS
 *************************************************************************/

// Initialise SI1133 light sensor.
ActionDriver si1133Init(char i2cAddress)
{
    ActionDriver result;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_OK;

    if (!gInitialised) {
        gI2cAddress = i2cAddress;

        data[0] = 0x0B; // REG_COMMAND
        data[1] = 0x01; // CMD_RESET

        // Do not access the Si1133 earlier than 25 ms from power-up
        Thread::wait(30);
        if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
            // Delay for 10 ms to allow the Si1133 to perform internal reset sequence
            Thread::wait(10);

            // Initialise the parameters
            for (unsigned int x = 0;
                 (x < ARRAY_SIZE(initPairs)) && ((result = setParameter(initPairs[x], initPairs[x + 1])) == ACTION_DRIVER_OK);
                 x += 2) {
            }

            data[0] = 0x0f; // REG_IRQ_ENABLE
            data[1] = 0x0f;
            if (result == ACTION_DRIVER_OK) {
                if (i2cSendReceive(gI2cAddress, data, 2, NULL, 0) == 0) {
                    gInitialised = true;
                } else {
                    result = ACTION_DRIVER_ERROR_I2C_WRITE;
                }
            }
        } else {
            result = ACTION_DRIVER_ERROR_I2C_WRITE;
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// Shut-down the SI1133 light sensor.
void si1133Deinit()
{
    MTX_LOCK(gMtx);

    if (gInitialised) {
        // Set PARAM_CH_LIST
        setParameter(0x01, 0x3f);
        // Send CMD_PAUSE_CH
        sendCommand(0x12);
        waitUntilSleep();

        gInitialised = false;
    }

    MTX_UNLOCK(gMtx);
}

// Read visible and UV light levels
ActionDriver getLight(int *pLux, int *pUvIndexX1000)
{
    ActionDriver result;
    Timer timer;
    Samples samples;
    char data[2];

    MTX_LOCK(gMtx);

    result = ACTION_DRIVER_ERROR_NOT_INITIALISED;

    if (gInitialised) {

        // Force a measurement
        result = sendCommand(0x11);

        if (result == ACTION_DRIVER_OK) {
            // Wait for the answer measurement to complete
            data[0] = 0x12; // REG_IRQ_STATUS
            data[1] = 0;
            result = ACTION_DRIVER_ERROR_I2C_WRITE_READ;
            timer.reset();
            timer.start();
            while ((i2cSendReceive(gI2cAddress, data, 1, &(data[1]), 1) == 1) &&
                   (data[1] != 0x0f) &&
                   (timer.read_ms() < SI1133_WAIT_FOR_READING_MS)) {
                Thread::wait(100); // Relax a little
            }
            timer.stop();

            // If we have a measurement, go get the result
            if (data[1] == 0x0f) {
                result = readResults(&samples);
                if (result == ACTION_DRIVER_OK) {
                    // Convert readings to lux
                    if (pLux != NULL) {
                        *pLux = (getLux(samples.ch1, samples.ch3, samples.ch2)) / (1 << LUX_OUTPUT_FRACTION);
                    }
                    // Convert readings to UV Index
                    if (pUvIndexX1000 != NULL) {
                        *pUvIndexX1000 = (getUvIndex(samples.ch0) * 1000) / (1 << UV_OUTPUT_FRACTION);
                    }
                }
            } else {
                if (timer.read_ms() >= SI1133_WAIT_FOR_READING_MS) {
                    result = ACTION_DRIVER_ERROR_TIMEOUT;
                }
            }
        }
    }

    MTX_UNLOCK(gMtx);

    return result;
}

// End of file
