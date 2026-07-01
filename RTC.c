#include "rtc.h"
#include <stdint.h>
#include <msp430.h>

// Convert BCD to decimal
static uint8_t bcd2dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

// Store total seconds since month start in big-endian order
void rtc_get_wvfm_time(uint8_t wvfm_time[4]) {
    // Hold RTC for consistent reading
    RTCCTL01 |= RTCHOLD;

    uint8_t day  = bcd2dec(RTCDAY);
    uint8_t hour = bcd2dec(RTCHOUR);
    uint8_t min  = bcd2dec(RTCMIN);
    uint8_t sec  = bcd2dec(RTCSEC);

    RTCCTL01 &= ~RTCHOLD;

    // Compute total seconds since the start of the month
    uint32_t total_seconds = ((uint32_t)(day - 1) * 24 * 60 * 60) +
                             ((uint32_t)hour * 3600) +
                             ((uint32_t)min * 60) +
                             (uint32_t)sec;

    // Store sequentially MSB first (big-endian)
    wvfm_time[0] = (uint8_t)((total_seconds >> 24) & 0xFF); // MSB
    wvfm_time[1] = (uint8_t)((total_seconds >> 16) & 0xFF);
    wvfm_time[2] = (uint8_t)((total_seconds >> 8) & 0xFF);
    wvfm_time[3] = (uint8_t)(total_seconds & 0xFF);         // LSB
}

//------------------------------------------------------------------------------
// Optional: RTC Initialization
//      Set RTC to September 2, 2025, 23:15:00
//           rtc_init(2025, 9, 2, 23, 15, 0);
//------------------------------------------------------------------------------
void rtc_init(uint16_t year, uint8_t month, uint8_t day,
              uint8_t hour, uint8_t min, uint8_t sec) {
RTCCTL01 = RTCHOLD | RTCSSEL_1 | RTCBCD;  // Hold RTC, select ACLK, BCD mode
RTCCTL23 = 0;                             // Optional: clear alarm/control registers

// Set the time/date
RTCYEAR  = ((year % 100)/10)<<4 | (year % 100)%10;
RTCMON   = (month /10)<<4 | (month %10);
RTCDAY   = (day /10)<<4 | (day %10);
RTCHOUR  = (hour/10)<<4 | (hour%10);
RTCMIN   = (min/10)<<4 | (min%10);
RTCSEC   = (sec/10)<<4 | (sec%10);

RTCCTL01 &= ~RTCHOLD;  // Start RTC
}
