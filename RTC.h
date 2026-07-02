#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// Initialize RTC to a specific date/time
void rtc_init(uint16_t year, uint8_t month, uint8_t day,
              uint8_t hour, uint8_t min, uint8_t sec);

void rtc_get_wvfm_time(uint8_t wvfm_time[4]);

#endif // RTC_H
