/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * Platform Specific part of Hardware interface Layer
 * ----------------------------------------------------------------------------
 */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "jshardware.h"
#include "jstimer.h"
#include "jsutils.h"
#include "jsparse.h"
#include "jsinteractive.h"
#include "jswrap_io.h"
#include "jswrap_date.h" // for non-F1 calendar -> days since 1970 conversion

#include <xc.h>
#include <sys/attribs.h>
#include "main.h"
#include "system_config.h"
#include "system_definitions.h"
#include "peripheral/usart/plib_usart.h"
#include "peripheral/devcon/plib_devcon.h"

#ifdef PIC32MZ_EF_SK_USART
//  #include "uart_interface.h"
#endif

#if 0
// ****************************************************************************
// ****************************************************************************
// Section: Configuration Bits
// ****************************************************************************
// ****************************************************************************

/*** DEVCFG0 ***/

#pragma config DEBUG =      ON
#pragma config JTAGEN =     OFF
#pragma config ICESEL =     ICS_PGx2
#pragma config TRCEN =      OFF
#pragma config BOOTISA =    MIPS32
#pragma config FECCCON =    OFF_UNLOCKED
#pragma config FSLEEP =     OFF
#pragma config DBGPER =     PG_ALL
#pragma config SMCLR =      MCLR_NORM
#pragma config SOSCGAIN =   GAIN_2X
#pragma config SOSCBOOST =  ON
#pragma config POSCGAIN =   GAIN_2X
#pragma config POSCBOOST =  ON
#pragma config EJTAGBEN =   NORMAL
#pragma config CP =         OFF

/*** DEVCFG1 ***/

#pragma config FNOSC =      SPLL
#pragma config DMTINTV =    WIN_127_128
#pragma config FSOSCEN =    OFF
#pragma config IESO =       OFF
#pragma config POSCMOD =    EC
#pragma config OSCIOFNC =   OFF
#pragma config FCKSM =      CSDCMD
#pragma config WDTPS =      PS1048576
#pragma config WDTSPGM =    STOP
#pragma config FWDTEN =     OFF
#pragma config WINDIS =     NORMAL
#pragma config FWDTWINSZ =  WINSZ_25
#pragma config DMTCNT =     DMT31
#pragma config FDMTEN =     OFF

/*** DEVCFG2 ***/

#pragma config FPLLIDIV =   DIV_3
#pragma config FPLLRNG =    RANGE_5_10_MHZ
#pragma config FPLLICLK =   PLL_POSC
#pragma config FPLLMULT =   MUL_50
#pragma config FPLLODIV =   DIV_2
#pragma config UPLLFSEL =   FREQ_24MHZ

/*** DEVCFG3 ***/

#pragma config USERID =     0xffff
#pragma config FMIIEN =     OFF
#pragma config FETHIO =     ON
#pragma config PGL1WAY =    OFF
#pragma config PMDL1WAY =   OFF
#pragma config IOL1WAY =    OFF
#pragma config FUSBIDIO =   OFF

/*** BF1SEQ0 ***/

#pragma config TSEQ =       0x0001
#pragma config CSEQ =       0xFFFE
#endif

#define CORE_TICK_RATE (F_CPU/2000000ul)

extern void __attribute__((nomips16, noreturn, far, weak)) __pic32_software_reset();
extern void SYS_Initialize ( void* data );
static int init = 0; // Temp hack to get jsiOneSecAfterStartup() going.

bool __pic32_WriteString(char* ptr)
{
  if (*ptr == '\0')
    return true;

    /* Write a character at a time, only if transmitter is empty */
    while (*ptr != '\0')
    {
      while(PLIB_USART_TransmitterBufferIsFull(USART_ID_2));
        /* Send character */
        PLIB_USART_TransmitterByteSend(USART_ID_2, *ptr);

        /* Increment to address of next character */
        ptr++;

    }
    return false;
}

static inline __pic32_init_core_timer(void)
{
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Configure the core timer
// clear the count reg
_CP0_SET_COUNT(0);
// set up the period in the compare reg
_CP0_SET_COMPARE(CORE_TICK_RATE);

// The Core timer should halt when we are halted at a debug breakpoint.
_CP0_BIC_DEBUG(_CP0_DEBUG_COUNTDM_MASK);

// set up the core timer interrupt with a priority of 6 and zero sub-priority
IFS0CLR = _IFS0_CTIF_MASK;
IPC0CLR = _IPC0_CTIP_MASK;
IPC0SET = (6 << _IPC0_CTIP_POSITION);
IPC0CLR = _IPC0_CTIS_MASK; 
IPC0SET = (0 << _IPC0_CTIS_POSITION);
IEC0CLR = _IEC0_CTIE_MASK;
IEC0SET = (1 << _IEC0_CTIE_POSITION);

}

// Now implement the Espruino HAL API...
void jshInit()
{
    JshUSARTInfo inf;
    PRISS = 0x76543210u;

    SYS_Initialize ( NULL );
    
    jshUSARTSetup(EV_SERIAL2,&inf);
    __pic32_init_core_timer();
    

    jshInitDevices();
    /* Maintain system services */
    SYS_DEVCON_Tasks(sysObj.sysDevcon);

    init = 1;
}

// When 'reset' is called - we try and put peripherals back to their power-on state
void jshReset() {

}

void jshKill() {
    __pic32_software_reset();
}

// stuff to do on idle
void jshIdle() {

  if (init == 1)
  {
    jsiOneSecondAfterStartup(); // Do this the first time we enter jshIdle() after we have called jshInit() and never again.
    init = 0;
  }
  jshUSARTKick(EV_SERIAL1);

}

/// Get this IC's serial number. Passed max # of chars and a pointer to write to. Returns # of chars
int jshGetSerialNumber(unsigned char *data, int maxChars) {
  return 0;
}

// is the serial device connected?
bool jshIsUSBSERIALConnected() {
  return false;
}

volatile JsSysTime SysTickValue = 0;
/// Get the system time (in ticks)
JsSysTime jshGetSystemTime()
{
  return SysTickValue;
}

/// Set the system time (in ticks) - this should only be called rarely as it could mess up things like jsinteractive's timers!
void jshSetSystemTime(JsSysTime newTime) {
  jshInterruptOff();
  SysTickValue = newTime;
  _CP0_SET_COUNT(0);
  _CP0_SET_COMPARE(CORE_TICK_RATE);
  jshInterruptOn();
  jshGetSystemTime(); // force update of the time
}
/// Convert a time in Milliseconds to one in ticks
JsSysTime jshGetTimeFromMilliseconds(JsVarFloat ms)
{
  return (JsSysTime)(ms * 1000);
}
/// Convert ticks to a time in Milliseconds
JsVarFloat jshGetMillisecondsFromTime(JsSysTime time)
{
  return ((JsVarFloat)time)/1000;
}

// software IO functions...
void jshInterruptOff() {
    __builtin_disable_interrupts();
}
void jshInterruptOn() {
    __builtin_enable_interrupts();
}
void jshDelayMicroseconds(int microsec) {
  JsSysTime initial = SysTickValue;
  if (microsec <= 0)
  {
    return; 
  }
  while(SysTickValue < (initial + microsec));
}

void jshPinSetValue(Pin pin, bool value) {
  if (value == 1)
  {
//    nrf_gpio_pin_set(pin);
  }
  else
  {
//    nrf_gpio_pin_clear(pin);
  }
}
bool jshPinGetValue(Pin pin) {
//  return nrf_gpio_pin_read(pin);
}
// ------

/// Set the pin state
void jshPinSetState(Pin pin, JshPinState state) {

}
/** Get the pin state (only accurate for simple IO - won't return JSHPINSTATE_USART_OUT for instance).
 * Note that you should use JSHPINSTATE_MASK as other flags may have been added */
JshPinState jshPinGetState(Pin pin) {
  return JSHPINSTATE_UNDEFINED;
}

// Returns an analog value between 0 and 1
JsVarFloat jshPinAnalog(Pin pin)
{
  return 0.0;
}
/// Returns a quickly-read analog value in the range 0-65535
int jshPinAnalogFast(Pin pin) {
  return 0;
}

JshPinFunction jshPinAnalogOutput(Pin pin, JsVarFloat value, JsVarFloat freq, JshAnalogOutputFlags flags) { // if freq<=0, the default is used
  return JSH_NOTHING;
}

void jshPinPulse(Pin pin, bool value, JsVarFloat time) {
  //return JSH_NOTHING;
}
///< Can the given pin be watched? it may not be possible because of conflicts
bool jshCanWatch(Pin pin) {
  return false;
}

IOEventFlags jshPinWatch(Pin pin, bool shouldWatch) {
  return EV_SERIAL2;
} // start watching pin - return the EXTI associated with it

/// Given a Pin, return the current pin function associated with it
JshPinFunction jshGetCurrentPinFunction(Pin pin) {
  return JSH_NOTHING;
}

/// Given a pin function, set that pin to the 16 bit value (used mainly for DACs and PWM)
void jshSetOutputValue(JshPinFunction func, int value) {

}

/// Enable watchdog with a timeout in seconds
void jshEnableWatchDog(JsVarFloat timeout) {

}

/** Check the pin associated with this EXTI - return true if it is a 1 */
bool jshGetWatchedPinState(IOEventFlags device) {
  return false;
}

bool jshIsEventForPin(IOEvent *event, Pin pin) {
  return false;
}

/** Is the given device initialised? */
bool jshIsDeviceInitialised(IOEventFlags device) {
  return false;
}

/** Set up a UART, if pins are -1 they will be guessed */
void jshUSARTSetup(IOEventFlags device, JshUSARTInfo *inf) {
  if (device == EV_USBSERIAL) {
    return; // eep!
  }
  else if (device != EV_SERIAL2)
  {
    jsExceptionHere(JSET_INTERNALERROR, "Unknown serial port device.");
    return;
  }
    PLIB_USART_Enable(USART_ID_2);
    __pic32_WriteString("\r\n\nEspruino on PIC32MZ MCU - Proof of Concept\r\n");
}
/** Kick a device into action (if required). For instance we may need
 * to set up interrupts */

void jshUSARTKick(IOEventFlags device) {

  if (device != EV_SERIAL2)
  {
	  return;
  }

  int check_valid_char = jshGetCharToTransmit(EV_SERIAL2);
  if (check_valid_char >= 0)
  {
    uint8_t character = (uint8_t) check_valid_char;
    while(PLIB_USART_TransmitterBufferIsFull(USART_ID_2));
    /* Send character */
    PLIB_USART_TransmitterByteSend(USART_ID_2, character);
  }

}

/** Set up SPI, if pins are -1 they will be guessed */
void jshSPISetup(IOEventFlags device, JshSPIInfo *inf) {

}
/** Send data through the given SPI device (if data>=0), and return the result
 * of the previous send (or -1). If data<0, no data is sent and the function
 * waits for data to be returned */
int jshSPISend(IOEventFlags device, int data) {
  return -1;
}
/** Send 16 bit data through the given SPI device. */
void jshSPISend16(IOEventFlags device, int data) {

}
/** Set whether to send 16 bits or 8 over SPI */
void jshSPISet16(IOEventFlags device, bool is16) {

}
/** Set whether to use the receive interrupt or not */
void jshSPISetReceive(IOEventFlags device, bool isReceive) {

}
/** Wait until SPI send is finished, and flush all received data */
void jshSPIWait(IOEventFlags device) {

}

/** Set up I2C, if pins are -1 they will be guessed */
void jshI2CSetup(IOEventFlags device, JshI2CInfo *inf) {

}
/** Addresses are 7 bit - that is, between 0 and 0x7F. sendStop is whether to send a stop bit or not */
void jshI2CWrite(IOEventFlags device, unsigned char address, int nBytes, const unsigned char *data, bool sendStop) {

}
void jshI2CRead(IOEventFlags device, unsigned char address, int nBytes, unsigned char *data, bool sendStop) {

}

/// Return start address and size of the flash page the given address resides in. Returns false if no page
bool jshFlashGetPage(uint32_t addr, uint32_t *startAddr, uint32_t *pageSize) {
  return false;
}
/// Erase the flash page containing the address
void jshFlashErasePage(uint32_t addr) {

}
/// Read data from flash memory into the buffer
void jshFlashRead(void *buf, uint32_t addr, uint32_t len) {

}
/// Write data to flash memory from the buffer
void jshFlashWrite(void *buf, uint32_t addr, uint32_t len) {

}

/// Save contents of JsVars into Flash
void jshSaveToFlash() {

}
/// Load contents of JsVars from Flash
void jshLoadFromFlash() {

}
/// Returns true if flash contains something useful
bool jshFlashContainsCode() {
  return false;
}

/// Enter simple sleep mode (can be woken up by interrupts). Returns true on success
bool jshSleep(JsSysTime timeUntilWake) {
//  __WFI(); // Wait for interrupt is a hint instruction that suspends execution until one of a number of events occurs.
  return true;
}

/// Utility timer handling functions ------------------------------

//const nrf_drv_timer_t TIMER_JSH = NRF_DRV_TIMER_INSTANCE(0);

/*void timer_event_handler(nrf_timer_event_t event_type, void * p_context)
{
  switch(event_type)
  {
    case NRF_TIMER_EVENT_COMPARE0:
      // Throw an interrupt.
      break;

    default:
      // Do nothing.
      break;
  }
}

void timer_init(JsSysTime period)
{
  uint32_t time_ticks;
  uint32_t err_code = NRF_SUCCESS;

  err_code = nrf_drv_timer_init(&TIMER_JSH, NULL, timer_event_handler);
  APP_ERROR_CHECK(err_code);

  time_ticks = nrf_drv_timer_ms_to_ticks(&TIMER_JSH, (uint32_t) period);

  nrf_drv_timer_extended_compare(&TIMER_JSH, NRF_TIMER_CC_CHANNEL0, time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);

  nrf_drv_timer_enable(&TIMER_JSH);
}*/

/// Start the timer and get it to interrupt after 'period'
void jshUtilTimerStart(JsSysTime period) {
}
/// Reschedult the timer (it should already be running) to interrupt after 'period'
void jshUtilTimerReschedule(JsSysTime period) {
}
/// Stop the timer
void jshUtilTimerDisable() {

}

// On SYSTick interrupt, call this
inline void __attribute__((always_inline)) jshDoSysTick() {
  /* Handle the delayed Ctrl-C -> interrupt behaviour (see description by EXEC_CTRL_C's definition)  */
  if (execInfo.execute & EXEC_CTRL_C_WAIT)
    execInfo.execute = (execInfo.execute & ~EXEC_CTRL_C_WAIT) | EXEC_INTERRUPTED;
  if (execInfo.execute & EXEC_CTRL_C)
    execInfo.execute = (execInfo.execute & ~EXEC_CTRL_C) | EXEC_CTRL_C_WAIT;
}

// the temperature from the internal temperature sensor
JsVarFloat jshReadTemperature()
{
  return 0.0;
}
// The voltage that a reading of 1 from `analogRead` actually represents
JsVarFloat jshReadVRef()
{
  return 0.0;
}
/* Get a random number - either using special purpose hardware or by
 * reading noise from an analog input. If unimplemented, this should
 * default to `rand()` */
unsigned int jshGetRandomNumber() {
  return 0;
}

void __ISR_AT_VECTOR(_CORE_TIMER_VECTOR, IPL6SRS) __attribute__((no_fpu)) CoreTimerHandler(void)
{
    unsigned long old_count, period;
    old_count = _CP0_GET_COUNT();
    
    SysTickValue++;
    jshDoSysTick();
    
    // clear the interrupt flag
    IFS0CLR = _IFS0_CTIF_MASK;

    // update the period
    period = CORE_TICK_RATE;
    period += old_count;
    _CP0_SET_COMPARE(period);
}


void __ISR_AT_VECTOR(_UART2_TX_VECTOR, IPL1SRS) __attribute__((no_fpu)) _IntHandlerDrvUsartTransmitInstance0(void)
{


    /* TODO: Add code to process interrupt here */

    /* Clear pending interrupt */
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_2_TRANSMIT);
}
void __ISR_AT_VECTOR(_UART2_RX_VECTOR, IPL1SRS) _IntHandlerDrvUsartReceiveInstance0(void)
{
    /* TODO: Add code to process interrupt here */

	if(PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_2_RECEIVE))
    {
      uint8_t data;
        /* Make sure receive buffer has data availible */
        if (PLIB_USART_ReceiverDataIsAvailable(USART_ID_2))
        {
            /* Get the data from the buffer */
            data = PLIB_USART_ReceiverByteReceive(USART_ID_2);
        }
        PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_2_RECEIVE);
        jshPushIOCharEvent(EV_SERIAL2, (char) data);
    }
    else
    {
        __builtin_software_breakpoint();
    }

}
void __ISR_AT_VECTOR(_UART2_FAULT_VECTOR, IPL1SRS) __attribute__((no_fpu)) _IntHandlerDrvUsartErrorInstance0(void)
{
    /* Clear pending interrupt */
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_2_ERROR);

}
