#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "wait_queues.h"
#include "../utils/util.h"

/* references
https://linux-kernel-labs.github.io/master/labs/deferred_work.html#workqueues
*/

/* wait queue declaration */
static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static int wait_queue_condition = 0;

/* work queue declaration */
static struct work_struct work;


/* sleep in process context */
static void work_queue_function(struct work_struct* work)
{
    PR_INFO("entry");

    msleep(5000);
    PR_INFO("sleep for 5000 msec");
    /* set conditional for wait queue release */
    wait_queue_condition = 1;
    wake_up_interruptible(&wait_queue);
}


void wait_queues_demo(void)
{
    PR_INFO("entry");

    /* init work queue */
    INIT_WORK(&work, work_queue_function);
    /* DECLARE_WORK() for declaration + initialization */
    schedule_work(&work);

    PR_INFO("sleep using wait queue");

    wait_event_interruptible(wait_queue, wait_queue_condition != 0);

    PR_INFO("wait queue awoke");
}
