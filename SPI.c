#include "SPI.h"
#include <msp430.h>
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// Extern Globals From main.c
///////////////////////////////////////////////////////////////////////////////

extern volatile uint16_t adc_buffer[];
extern volatile uint16_t adc_index;
extern const uint16_t ADC_BUFFER_SIZE;

extern uint8_t sensor_ID;
extern uint8_t CS_ID;
extern uint8_t wvfm_freq[4];
extern uint8_t wvfm_time[4];        // <-- Added
extern const uint8_t wvfm_start[9];
extern const uint8_t wvfm_finish[10];

///////////////////////////////////////////////////////////////////////////////
// CS Pin Definitions
///////////////////////////////////////////////////////////////////////////////

#define SLAVE_CS_IN   P2IN
#define SLAVE_CS_DIR  P2DIR
#define SLAVE_CS_PIN  BIT0

///////////////////////////////////////////////////////////////////////////////
// SPI State Machine
///////////////////////////////////////////////////////////////////////////////

typedef enum
{
    RX_CMD_MODE,
    TX_DATA_MODE
} SPI_State;

typedef enum
{
    TX_START,
    TX_SENSOR_ID,
    TX_CS_ID,
    TX_FREQ,
    TX_TIME,      // <-- Added
    TX_ADC,
    TX_FINISH
} TX_Phase;

static volatile SPI_State spiMode = RX_CMD_MODE;
static volatile TX_Phase  txPhase = TX_START;

static volatile uint16_t TXIndex   = 0;
static volatile uint8_t  TXSub     = 0;
static volatile uint16_t metaIndex = 0;

///////////////////////////////////////////////////////////////////////////////
// Low-Level Send Byte
///////////////////////////////////////////////////////////////////////////////

static void sendSPIByte(uint8_t val)
{
    while (!(UCA0IFG & UCTXIFG));
    UCA0TXBUF = val;
}

///////////////////////////////////////////////////////////////////////////////
// Process Command
///////////////////////////////////////////////////////////////////////////////

static void processCMD(uint8_t cmd)
{
    switch (cmd)
    {
        case CMD_ADC_READY:

            if (adc_index < ADC_BUFFER_SIZE)
                sendSPIByte(BUSY_BYTE);
            else
                sendSPIByte(READY_BYTE);

            break;

        case CMD_ADC_DATA:

            spiMode   = TX_DATA_MODE;
            txPhase   = TX_START;
            metaIndex = 0;

            TXIndex = 0;
            TXSub   = 0;

            break;

        default:
            sendSPIByte(DUMMY);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
// SPI ISR
///////////////////////////////////////////////////////////////////////////////

#pragma vector=USCI_A0_VECTOR
__interrupt void SPI_ISR(void)
{
    uint8_t rx = UCA0RXBUF;

    switch (spiMode)
    {
        /////////////////////////////////////////////////////////////////////////
        // Command Mode
        /////////////////////////////////////////////////////////////////////////
        case RX_CMD_MODE:
            processCMD(rx);
            break;

        /////////////////////////////////////////////////////////////////////////
        // Transmission Mode
        /////////////////////////////////////////////////////////////////////////
        case TX_DATA_MODE:

            switch (txPhase)
            {
                /////////////////////////////////////////////////////////////////
                // wvfm_start[]
                /////////////////////////////////////////////////////////////////
                case TX_START:

                    sendSPIByte(wvfm_start[metaIndex++]);

                    if (metaIndex >= 9)
                        txPhase = TX_SENSOR_ID;

                    break;

                /////////////////////////////////////////////////////////////////
                // sensor_ID
                /////////////////////////////////////////////////////////////////
                case TX_SENSOR_ID:

                    sendSPIByte(sensor_ID);
                    txPhase = TX_CS_ID;

                    break;

                /////////////////////////////////////////////////////////////////
                // CS_ID
                /////////////////////////////////////////////////////////////////
                case TX_CS_ID:

                    sendSPIByte(CS_ID);
                    txPhase   = TX_FREQ;
                    metaIndex = 0;

                    break;

                /////////////////////////////////////////////////////////////////
                // wvfm_freq[4]
                /////////////////////////////////////////////////////////////////
                case TX_FREQ:

                    sendSPIByte(wvfm_freq[metaIndex++]);

                    if (metaIndex >= 4)
                    {
                        txPhase   = TX_TIME;   // <-- Move to TIME
                        metaIndex = 0;
                    }

                    break;

                /////////////////////////////////////////////////////////////////
                // wvfm_time[4]
                /////////////////////////////////////////////////////////////////
                case TX_TIME:

                    sendSPIByte(wvfm_time[metaIndex++]);

                    if (metaIndex >= 4)
                        txPhase = TX_ADC;

                    break;

                /////////////////////////////////////////////////////////////////
                // ADC samples (big-endian)
                /////////////////////////////////////////////////////////////////
                case TX_ADC:

                    if (TXIndex < ADC_BUFFER_SIZE)
                    {
                        uint8_t byte_to_send;

                        if (TXSub == 0)
                        {
                            byte_to_send = (uint8_t)(adc_buffer[TXIndex] >> 8);
                            TXSub = 1;
                        }
                        else
                        {
                            byte_to_send = (uint8_t)(adc_buffer[TXIndex] & 0xFF);
                            TXSub = 0;
                            TXIndex++;
                        }

                        sendSPIByte(byte_to_send);
                    }
                    else
                    {
                        txPhase   = TX_FINISH;
                        metaIndex = 0;
                    }

                    break;

                /////////////////////////////////////////////////////////////////
                // wvfm_finish[]
                /////////////////////////////////////////////////////////////////
                case TX_FINISH:

                    sendSPIByte(wvfm_finish[metaIndex++]);

                    if (metaIndex >= 10)
                        spiMode = RX_CMD_MODE;

                    break;
            }

            break;

        default:
            sendSPIByte(DUMMY);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////
// SPI Initialization (Slave Mode)
///////////////////////////////////////////////////////////////////////////////

void SPI_init(void)
{
    SLAVE_CS_DIR &= ~SLAVE_CS_PIN;

    P3SEL |= BIT3 + BIT4;
    P2SEL |= BIT7;

    UCA0CTL1 = UCSWRST;
    UCA0CTL0 = UCMSB | UCCKPL | UCSYNC;
    UCA0CTL1 &= ~UCSWRST;

    UCA0IE |= UCRXIE;
}