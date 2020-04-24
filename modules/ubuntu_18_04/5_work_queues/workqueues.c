#include <linux/cpumask.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "../utils/util.h"

/* references
https://lwn.net/Articles/11360/
https://lwn.net/Articles/211279/
https://www.kernel.org/doc/html/v4.14/core-api/workqueue.html
*/


static struct workqueue_struct* workqueue_;


struct Data {
    struct work_struct work;
    int value;
} *data_;

struct DataDelayed {
    struct delayed_work work;
    int value;
} *data_delayed_2_, *data_delayed_4_, *data_delayed_6_;

static void do_work(struct work_struct* work_arg)
{
    PR_INFO("entry");

    {
        /* get container to oobtain data payload */
        struct Data* data_container = container_of(work_arg, struct Data, work);
        PR_INFO("value = %d", data_container->value);
        /* delelete Data created in init */
        kfree(data_container);
    }
}

static void do_work_delayed(struct work_struct* work_arg)
{
    PR_INFO("entry");

    {
        /* get container to oobtain data payload */
        struct delayed_work* delayed_work = to_delayed_work(work_arg);
        struct DataDelayed* data_container = container_of(delayed_work, struct DataDelayed, work);
        PR_INFO("value = %d", data_container->value);
        /* delelete Data created in init */
        kfree(data_container);
    }
}

void workqueues_init(void)
{
    int i;
    // int num_cores = num_online_cpus();

    PR_INFO("entry");
    // PR_INFO("%d cpus", num_cores);

    workqueue_ = create_singlethread_workqueue("single_threaded_work_queue");

    data_  = kmalloc(sizeof(struct Data), GFP_KERNEL);
    data_->value = 0;
    INIT_WORK(&data_->work, do_work);
    queue_work(workqueue_, &data_->work);

    for (i = 1; i < 5; i++) {
        struct DataDelayed* data_delayed  = kmalloc(sizeof(struct DataDelayed), GFP_KERNEL);
        data_delayed->value = i;
        INIT_DELAYED_WORK(&data_delayed->work, do_work_delayed);
        queue_delayed_work(workqueue_, &data_delayed->work, msecs_to_jiffies(i*1000));
    }

    // data_delayed_2_  = kmalloc(sizeof(struct DataDelayed), GFP_KERNEL);
    // data_delayed_2_->value = 2;
    // INIT_DELAYED_WORK(&data_delayed_2_->work, do_work_delayed);
    // queue_delayed_work(workqueue_, &data_delayed_2_->work, msecs_to_jiffies(2000));

    // data_delayed_4_  = kmalloc(sizeof(struct DataDelayed), GFP_KERNEL);
    // data_delayed_4_->value = 4;
    // INIT_DELAYED_WORK(&data_delayed_4_->work, do_work_delayed);
    // queue_delayed_work(workqueue_, &data_delayed_4_->work, msecs_to_jiffies(4000));

    // data_delayed_6_  = kmalloc(sizeof(struct DataDelayed), GFP_KERNEL);
    // data_delayed_6_->value = 6;
    // INIT_DELAYED_WORK(&data_delayed_6_->work, do_work_delayed);
    // queue_delayed_work(workqueue_, &data_delayed_6_->work, msecs_to_jiffies(6000));
}


void workqueues_exit(void)
{
    PR_INFO("entry");

    PR_INFO("flush and destroy workqueue");
    flush_workqueue(workqueue_);
    destroy_workqueue(workqueue_);
}


void workqueues_open(void)
{
    PR_INFO("entry");
}


void workqueues_close(void)
{
    PR_INFO("entry");
}
