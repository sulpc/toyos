#include "util_time.h"


static inline bool is_leap_year(uint16_t year) {
    if (year % 4 == 0) {
        if (year % 100 == 0) {
            if (year % 400 == 0)
                return true;
            else
                return false;
        } else
            return true;
    } else
        return false;
}


void util_localtime(uint32_t timestamp, struct util_time_t* tmp) {
    const uint32_t secs_min  = 60;
    const uint32_t secs_hour = 3600;
    const uint32_t secs_day  = 3600 * 24;

    uint32_t days    = timestamp / secs_day; /* Days passed since epoch. */
    uint32_t seconds = timestamp % secs_day; /* Remaining seconds. */

    tmp->tm_hour = seconds / secs_hour;
    tmp->tm_min  = (seconds % secs_hour) / secs_min;
    tmp->tm_sec  = (seconds % secs_hour) % secs_min;

    /* Calculate the current year. */
    tmp->tm_year = 1970;

    while (1) {
        /* Leap years have one day more. */
        uint32_t days_this_year = 365 + is_leap_year(tmp->tm_year);
        if (days_this_year > days)
            break;
        days -= days_this_year;
        tmp->tm_year++;
    }

    /* We need to calculate in which month and day of the month we are. To do * so we need to skip days according to how
     * many days there are in each * month, and adjust for the leap year that has one more day in February. */
    uint8_t mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    mdays[1] += is_leap_year(tmp->tm_year);

    tmp->tm_mon = 0;
    while (days >= mdays[tmp->tm_mon]) {
        days -= mdays[tmp->tm_mon];
        tmp->tm_mon++;
    }

    tmp->tm_mday = days + 1; /* Add 1 since our 'days' is zero-based. */
    tmp->tm_year -= 1900;    /* Surprisingly tm_year is year-1900. */
}


/**
 * @brief unix timestamp, seconds from 1970
 *
 * @param tm
 * @return uint32_t
 */
uint32_t util_mktime(struct util_time_t* tm) {
    uint32_t seccount = 0;

    if (tm->tm_year < 1970 || tm->tm_year > 2099)
        return 0;

    // seconds before current year
    for (uint16_t y = 1970; y < tm->tm_year; y++) {
        if (is_leap_year(y))
            seccount += 31622400;   // leap_year, 366 days
        else
            seccount += 31536000;   // 265 days
    }

    // seconds before current month
    const uint8_t mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t       mon       = tm->tm_mon;
    if (is_leap_year(tm->tm_year) && mon > 2) {
        seccount += 86400;   // leap_year, +1 day in February
    }
    while (mon > 1) {
        mon--;
        seccount += (uint32_t)mdays[mon] * 86400;
    }

    // seconds before current day
    seccount += (uint32_t)(tm->tm_mday - 1) * 86400;

    // seconds...
    seccount += (uint32_t)tm->tm_hour * 3600;
    seccount += (uint32_t)tm->tm_min * 60;
    seccount += tm->tm_sec;

    return seccount;
}
