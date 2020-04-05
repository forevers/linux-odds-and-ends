
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include "timers.h"
#include "../utils/util.h"

/* references
*/


static struct timer_list one_shot_timer_;
static struct timer_list periodic_timer_;

/* high resolution timer data structure */
static struct HrtData {
    struct hrtimer timer;
    ktime_t period;
    long period_sec;
    unsigned long period_nsec;
} *hrt_one_shot_data_, *hrt_periodic_data_;


static void one_shot_func(unsigned long data)
{
    PR_INFO("entry");
}
static enum hrtimer_restart one_shot_func_hrt(struct hrtimer* hr_timer)
{
    PR_INFO("entry");
    return HRTIMER_NORESTART;
}


static void periodic_func(unsigned long data)
{
    PR_INFO("entry");
    mod_timer(&periodic_timer_, jiffies + msecs_to_jiffies(1000));
}
static enum hrtimer_restart periodic_func_hrt(struct hrtimer* hr_timer)
{
    ktime_t now , interval;
    PR_INFO("entry");

    now  = ktime_get();
    interval = ktime_set(hrt_periodic_data_->period_sec, hrt_periodic_data_->period_nsec);
    hrtimer_forward(hr_timer, now , interval);

    return HRTIMER_RESTART;
}


void timers_init(void)
{
    PR_INFO("entry");

    hrt_one_shot_data_ = kmalloc(sizeof(*hrt_one_shot_data_), GFP_KERNEL);
    hrt_one_shot_data_->period_sec = 1;
    hrt_one_shot_data_->period_nsec = 1;
    hrt_one_shot_data_->period = ktime_set(hrt_one_shot_data_->period_sec, hrt_one_shot_data_->period_nsec);
    hrtimer_init(&hrt_one_shot_data_->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
    hrt_one_shot_data_->timer.function = one_shot_func_hrt;
    hrtimer_start(&hrt_one_shot_data_->timer, hrt_one_shot_data_->period, HRTIMER_MODE_REL);

    hrt_periodic_data_ = kmalloc(sizeof(*hrt_periodic_data_), GFP_KERNEL);
    hrt_periodic_data_->period_sec = 1;
    hrt_periodic_data_->period_nsec = 1;
    hrt_periodic_data_->period = ktime_set(hrt_periodic_data_->period_sec, hrt_periodic_data_->period_nsec);
    hrtimer_init(&hrt_periodic_data_->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
    hrt_periodic_data_->timer.function = periodic_func_hrt;
    hrtimer_start(&hrt_periodic_data_->timer, hrt_periodic_data_->period, HRTIMER_MODE_REL);

    setup_timer(&one_shot_timer_, one_shot_func, 0);
    setup_timer(&periodic_timer_, periodic_func, 0);

    mod_timer(&one_shot_timer_, jiffies + msecs_to_jiffies(1000));
    mod_timer(&periodic_timer_, jiffies + msecs_to_jiffies(2000));
}


void timers_exit(void)
{
    PR_INFO("entry");

    /* timer resources */
    PR_INFO("cancel active timers");
    if (hrtimer_cancel(&hrt_one_shot_data_->timer)) {
        PR_INFO("cancelled hrt_one_shot_data_ timer");
    }
    PR_INFO("free hrt_one_shot_data_ object\n");
    kfree(hrt_one_shot_data_);

    if (hrtimer_cancel(&hrt_periodic_data_->timer)) {
        PR_INFO("cancelled my_hrt_periodic_data_ timer");
    }
    PR_INFO("free hrt_periodic_data_ object\n");
    kfree(hrt_periodic_data_);

    if (del_timer_sync(&one_shot_timer_)) PR_INFO("one shot timer killed");
    if (del_timer_sync(&periodic_timer_)) PR_INFO("periodic timer_ killed");
}


void timers_open(void)
{
    PR_INFO("entry");
}


void timers_close(void)
{
    PR_INFO("entry");
}
