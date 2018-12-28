/* mn_time.h
 */

#ifndef MN_TIME_H
#define MN_TIME_H

#include <stdint.h>

int64_t mn_time_now();
void mn_time_sleep(int us);

#define MN_CLOCKASSIST_MODE_AUTO     1
#define MN_CLOCKASSIST_MODE_MANUAL   2

struct mn_clockassist {
  int mode;
  int64_t next;
  int delay;
  int cyclep;
  int64_t cycle_start_time;
  int64_t cycle_sleep_time;
  int64_t cycle_expect;
  int64_t manual_engagement_threshold;
  int64_t autopilot_engagement_threshold;
  int64_t framec_manual;
  int64_t framec_autopilot;
  int64_t framec;
  int64_t start;
};

int mn_clockassist_setup(struct mn_clockassist *clockassist,int rate_hz);

int mn_clockassist_update(struct mn_clockassist *clockassist);

#endif
