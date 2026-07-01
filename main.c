#include "msp430.h"
#include "SPI.h"
#include <stdint.h>
#include <string.h>

#define SMCLKFreq       16000000.0      // Updated from 1 MHz to 16 MHz


///////////////////////////////////////////////////////////////////////////////
// Global Variables
///////////////////////////////////////////////////////////////////////////////
volatile uint16_t samp_start, samp_end;
volatile float samp_freq;
const uint16_t ADC_BUFFER_SIZE = 32;
volatile uint16_t adc_buffer[ADC_BUFFER_SIZE];
volatile uint16_t adc_index;
volatile uint16_t adc_value;
float samp_freqs[ADC_BUFFER_SIZE] = {0};
uint16_t samp_index = 0;
float avg_samp_freq = 0.0f;

uint8_t wvfm_freq[4];    // 4-byte big-endian array
uint8_t CS_ID = 0x01;     //To be handled by AM263 later
uint8_t sensor_ID = 0x01; //To be set by configuration later in AIO section
uint8_t wvfm_time[4] = {0x01,0x02,0x03,0x04};
const uint8_t wvfm_start[9] = {0x77, 0x76, 0x66, 0x6D, 0x73, 0x74, 0x61, 0x72, 0x74};
const uint8_t wvfm_finish[10] = {0x77, 0x76, 0x66, 0x6D, 0x66, 0x69, 0x6E, 0x69, 0x73, 0x68};

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////
void configureADC(void);
void configureClocks(void);

///////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog

    configureClocks();                  // *** New: bump DCO to 16 MHz ***

    TA0CTL = TASSEL_2 | MC_2 | TACLR;   // Timer A continuous mode, SMCLK src
    //rtc_init(2025, 9, 2, 23, 15, 0);

    configureADC();
    SPI_init();

    __enable_interrupt();

    adc_index = 0;

    // Start ADC
    ADC12CTL0 &= ~ADC12ENC;
    ADC12CTL0 |= ADC12ENC;
    ADC12CTL0 |= ADC12SC;

    while (1)
    {
        __no_operation(); // ISR-driven main loop
    }
}

///////////////////////////////////////////////////////////////////////////////
// Clock Configuration (new)
///////////////////////////////////////////////////////////////////////////////
void configureClocks(void)
{
    UCSCTL3 = SELREF__REFOCLK;              // Set DCO FLL reference = REFO
    UCSCTL4 |= SELA__REFOCLK;               // ACLK = REFO
    __bis_SR_register(SCG0);                // Disable FLL control loop
    UCSCTL0 = 0x0000;                       // Set lowest possible DCOx, MODx
    UCSCTL1 = DCORSEL_5;                    // Select DCO range for 16 MHz
    UCSCTL2 = FLLD_1 + 487;                 // (487+1)*32768 ≈ 16 MHz
    __bic_SR_register(SCG0);                // Enable FLL control loop

    __delay_cycles(500000);                 // Allow DCO to settle
}

///////////////////////////////////////////////////////////////////////////////
// ADC Configuration
///////////////////////////////////////////////////////////////////////////////
void configureADC(void)
{
    ADC12CTL0 = ADC12SHT0_4 | ADC12ON | ADC12MSC;
    ADC12CTL1 = ADC12SHP | ADC12CONSEQ_2;  // Repeated single-channel mode
    ADC12CTL2 = ADC12RES_2;
    ADC12MCTL0 = ADC12INCH_1;
    ADC12IE = ADC12IE0;

    REFCTL0 = REFON | REFVSEL_2;   // 2.5V internal reference
    __delay_cycles(1000);
}

///////////////////////////////////////////////////////////////////////////////
// ADC12 ISR
///////////////////////////////////////////////////////////////////////////////
#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR(void)
{
    switch (__even_in_range(ADC12IV, 34))
    {
        case 6: // ADC12MEM0
            samp_end   = TA0R;
            adc_value  = ADC12MEM0;

            uint16_t ticks;
            if (samp_end >= samp_start)
                ticks = samp_end - samp_start;
            else
                ticks = (0xFFFF - samp_start) + samp_end + 1;

            samp_freq = (float)SMCLKFreq / ticks;
            samp_start = samp_end;

            if (adc_index < ADC_BUFFER_SIZE)
            {
                adc_buffer[adc_index++] = adc_value;
                samp_freqs[samp_index++] = samp_freq;
                if (samp_index >= ADC_BUFFER_SIZE) samp_index = 0;
            }

            if (adc_index >= ADC_BUFFER_SIZE)
            {
                // Buffer full, stop ADC
                ADC12CTL0 &= ~ADC12ENC;

                // Compute average sample frequency
                float sum = 0.0f;
                uint16_t k = 0;
                while (k < ADC_BUFFER_SIZE)
                {
                    sum += samp_freqs[k];
                    k++;
                }
                avg_samp_freq = sum / ADC_BUFFER_SIZE;
                // Copy float bits into a uint32_t
                uint32_t as_int;
                memcpy(&as_int, &avg_samp_freq, sizeof(float));  //Needs to be optimized
                // Store in big-endian (MSB first)
                wvfm_freq[0] = (uint8_t)((as_int >> 24) & 0xFF); // MSB
                wvfm_freq[1] = (uint8_t)((as_int >> 16) & 0xFF);
                wvfm_freq[2] = (uint8_t)((as_int >> 8) & 0xFF);
                wvfm_freq[3] = (uint8_t)(as_int & 0xFF);         // LSB
            }
            break;

        default:
            break;
    }
}