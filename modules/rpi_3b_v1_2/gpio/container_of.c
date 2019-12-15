#include <linux/kernel.h>
#include <stddef.h>

#include "container_of.h"
#include "util.h"

/* container_of */
struct Container {
    struct Container* next_container;
    int data_a;
    int data_b;
};


void container_demo(void)
{
    struct Container container;
    struct Container next_container;
    struct Container* iter;

    container.data_a = 1;
    container.data_b = 11;
    container.next_container = &next_container;

    next_container.data_a = 2;
    next_container.data_b = 22;
    next_container.next_container = NULL;

    iter = &container;
    while (iter != NULL) {
        int* data;
        struct Container* this_container;
        PR_INFO("iter->data_a : %d", iter->data_a);

        /* container_of example 
            obtain container reference and verify its value
        */
        data = &iter->data_a;
        this_container = container_of(data, struct Container, data_a);
        PR_INFO("this_container->data_a : %d", this_container->data_a);

        /* advance list iterator */
        iter = iter->next_container;
    }
}
