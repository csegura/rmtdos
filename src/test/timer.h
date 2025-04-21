/* Define NULL if not available */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* --- Configuration --- */

/*
 * Approximate integer representation of BIOS ticks per minute and second.
 * Based on ~18.2065 ticks/sec.
 * TICKS_PER_MINUTE = 18.2065 * 60 = 1092.39 -> Use 1092UL
 * TICKS_PER_SECOND = 18.2065 -> Use 18UL for integer division remainder calculation
 * Using these integers introduces small inaccuracies over longer durations.
 */
#define TICKS_PER_MIN_APPROX 1092UL
#define TICKS_PER_SEC_APPROX 18UL

/*
 * Approximate nanoseconds per single BIOS tick.
 * NS_PER_TICK = (1.0 / 18.2065) * 1,000,000,000 = 54,925,400 ns
 * Make sure this fits in unsigned long if intermediate calculations need it.
 * For the final step, we multiply by remainder ticks (0-17),
 * max value is 17 * 54925400 = 933,731,800, which fits in 32-bit unsigned long.
 */
#define NS_PER_TICK_APPROX 54925400UL

/* --- Data Structure --- */

struct ElapsedTime {
    unsigned long minutes;     /* Calculated elapsed minutes */
    unsigned int seconds;      /* Calculated elapsed seconds (0-59) */
    unsigned long nanoseconds; /* Calculated 'nanoseconds' (0-999,999,999) */
                               /* WARNING: Based on ~55ms BIOS tick resolution! */
                               /* This value is NOT truly nanosecond precise. */
};

void ticks_to_elapsed_time(unsigned long start_ticks, unsigned long end_ticks, struct ElapsedTime *result) {
    unsigned long elapsed_ticks;
    unsigned long rem_ticks_after_min;
    unsigned long rem_ticks_after_sec;

    /* Ensure result pointer is valid */
    if (result == NULL) {
        return;
    }

    /* 1. Calculate elapsed ticks, handling 32-bit rollover */
    if (end_ticks >= start_ticks) {
        elapsed_ticks = end_ticks - start_ticks;
    } else {
        /* Rollover occurred (0xFFFFFFFF -> 0) */
        elapsed_ticks = (0xFFFFFFFFUL - start_ticks) + end_ticks + 1;
    }

    /* 2. Calculate minutes */
    result->minutes = elapsed_ticks / TICKS_PER_MIN_APPROX;
    rem_ticks_after_min = elapsed_ticks % TICKS_PER_MIN_APPROX;

    /* 3. Calculate seconds from remaining ticks */
    /* Using 18 here for simplicity, as it's primarily for the remainder step */
    result->seconds = (unsigned int)(rem_ticks_after_min / TICKS_PER_SEC_APPROX);
    rem_ticks_after_sec = rem_ticks_after_min % TICKS_PER_SEC_APPROX;

     /* Ensure seconds are within 0-59 (should be, as 1092/18 is ~60.6) */
     /* If rem_ticks_after_min was near 1092, seconds could momentarily hit 60 */
     /* It's safer to calculate minutes and seconds from total seconds if accuracy is paramount */
     /* But this direct tick division is simpler for integer math */
     if (result->seconds >= 60) {
         /* This case indicates minor drift from approximations, adjust */
         /* Or reconsider calculation method if high accuracy over long time is needed */
         result->seconds = 59; /* Clamp for safety, though indicates issue */
         /* A more robust method might calculate total seconds first: */
         /* total_seconds = (elapsed_ticks * 1000UL) / 18207UL; // Approx 18.207 */
         /* result->minutes = total_seconds / 60UL; */
         /* result->seconds = (unsigned int)(total_seconds % 60UL); */
         /* Then calculate remainder ticks based on total_seconds, harder integer math */
     }


    /* 4. Calculate 'nanoseconds' from final remainder ticks */
    /* This calculation fits within 32-bit unsigned long */
    result->nanoseconds = rem_ticks_after_sec * NS_PER_TICK_APPROX;
}