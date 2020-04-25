#include <linux/cpumask.h>
#include <linux/getcpu.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp.h> 
#include <linux/workqueue.h>

#include "../utils/util.h"

/* references
https://lwn.net/Articles/11360/
https://lwn.net/Articles/211279/
https://www.kernel.org/doc/html/v4.14/core-api/workqueue.html
*/

/* workqueue */
static struct workqueue_struct* workqueue_;

/* work struct */
struct Data {
    struct work_struct work;
    int value;
} *data_;

/* delayed work struct */
struct DataDelayed {
    struct delayed_work work;
    int value;
    /* list maintained to cancel work tasks */
    struct list_head list;
} *data_delayed_2_, *data_delayed_4_, *data_delayed_6_;

/* work list to manage queued work items*/
LIST_HEAD(work_list_);


/* non-delayed work item processing */
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

/* delayed work item processing */
static void do_work_delayed(struct work_struct* work_arg)
{
    PR_INFO("entry");

    {
        int cpu = smp_processor_id();
        /* get container to oobtain data payload */
        struct delayed_work* delayed_work = to_delayed_work(work_arg);
        struct DataDelayed* data_container = container_of(delayed_work, struct DataDelayed, work);
        PR_INFO("value = %d, cpu = %d", data_container->value, cpu);
        /* remove work item from list */
        list_del(&data_container->list);
        /* delelete Data created in init */
        kfree(data_container);
    }
}

/* module init */
void workqueues_init(void)
{
    int i;
    int num_cores = num_online_cpus();

    PR_INFO("entry");
    PR_INFO("%d cpus", num_cores);

    workqueue_ = create_singlethread_workqueue("single_threaded_work_queue");

    data_  = kmalloc(sizeof(struct Data), GFP_KERNEL);
    data_->value = 0;
    INIT_WORK(&data_->work, do_work);
    queue_work(workqueue_, &data_->work);

    for (i = 0; i < num_cores; i++) {
        struct DataDelayed* data_delayed = kmalloc(sizeof(struct DataDelayed), GFP_KERNEL);
        data_delayed->value = i;
        INIT_DELAYED_WORK(&data_delayed->work, do_work_delayed);
        queue_delayed_work_on(i, workqueue_, &data_delayed->work, msecs_to_jiffies(i*1000));

        list_add(&data_delayed->list, &work_list_);
    }
}

/* module exit */
void workqueues_exit(void)
{
    struct list_head* safe_iter_temp;
    struct list_head* safe_iter_pos;
    struct DataDelayed* data_delayed_node;

    PR_INFO("entry");

    list_for_each_safe(safe_iter_pos, safe_iter_temp, &work_list_) {
        data_delayed_node = list_entry(safe_iter_pos, struct DataDelayed, list);
        cancel_delayed_work(&data_delayed_node->work);
        list_del(&data_delayed_node->list);
        PR_INFO("delist and delete work item %d ", data_delayed_node->value);
        kfree(data_delayed_node);
    }

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
