#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <stddef.h>

#include "linked_list.h"
#include "util.h"

/* linked list node */
struct LinkedListNode {
    int number_;
    struct list_head list_;
};


void linked_list_demo(void)
{
    int i;
    int num_elements = 10;
    struct LinkedListNode** nodes;
    struct LinkedListNode* head_node;

    if (NULL != (head_node = (struct LinkedListNode*)kzalloc(sizeof(struct LinkedListNode), GFP_KERNEL))) {
        head_node->number_ = -1;
        INIT_LIST_HEAD(&head_node->list_);

        /* construct a list */
        nodes = (struct LinkedListNode**)kzalloc(num_elements*sizeof(struct LinkedListNode*), GFP_KERNEL);
        for (i = 0; i < num_elements; i++) {
            if (NULL != (nodes[i] = (struct LinkedListNode*)kzalloc(sizeof(struct LinkedListNode), GFP_KERNEL))) {
                nodes[i]->number_ = i;
                // list_add(&(nodes[i]->list_), &(head_node->list_));
            } else {
                PR_INFO("  nodes[%d] number_ kzalloc() failure", i);
                /* release resources */
                while ((i-1) > 0) {
                    i--;
                    kfree(nodes[i]);
                }
                kfree(nodes);
                return;
            }
        }

        /* head insertion */
        {
            struct LinkedListNode* node_iter;
            struct LinkedListNode* linked_list_node;
            struct list_head* safe_iter_temp;
            struct list_head* safe_iter_pos;

            PR_INFO("***** HEAD INSERTION TEST ***** ");

            /* fill the list from the head */
            for (i = 0; i < num_elements; i++) {
                list_add(&(nodes[i]->list_), &(head_node->list_));
            }

            /* iterate the array */
            PR_INFO("nodes[] iteration : ");
            for (i = 0; i < num_elements; i++) {
                PR_INFO("  nodes[%d] number_ : %d", i, nodes[i]->number_);
            }
            /* iterate the list */
            list_for_each_entry(node_iter, &(head_node->list_), list_) {
                PR_INFO("node number : %d", node_iter->number_);
            }

            /* delete 5th node */
            PR_INFO("delete 5th node");
            list_del(&(nodes[5]->list_));

            /* iterate the array */
            PR_INFO("nodes[] iteration : ");
            for (i = 0; i < num_elements; i++) {
                PR_INFO("  nodes[%d] number_ : %d", i, nodes[i]->number_);
            }
            /* iterate the list */
            list_for_each_entry(node_iter, &(head_node->list_), list_) {
                PR_INFO("node number : %d", node_iter->number_);
            }
            /* remove all elements from list
               the _safe version allow for node removal from list while iterating list
            */
            list_for_each_safe(safe_iter_pos, safe_iter_temp, &(head_node->list_)) {
                linked_list_node = list_entry(safe_iter_pos, struct LinkedListNode, list_);
                list_del(&linked_list_node->list_);
            }
        }

        /* tail insertion */
        {
            struct LinkedListNode* node_iter;
            struct LinkedListNode* linked_list_node;
            struct list_head* safe_iter_temp;
            struct list_head* safe_iter_pos;

            PR_INFO("***** TAIL INSERTION TEST ***** ");

            /* fill the list from the tail */
            for (i = 0; i < num_elements; i++) {
                list_add_tail(&(nodes[i]->list_), &(head_node->list_));
            }

            /* iterate the array */
            PR_INFO("nodes[] iteration : ");
            for (i = 0; i < num_elements; i++) {
                PR_INFO("  nodes[%d] number_ : %d", i, nodes[i]->number_);
            }
            /* iterate the list */
            list_for_each_entry(node_iter, &(head_node->list_), list_) {
                PR_INFO("node number : %d", node_iter->number_);
            }

            /* delete 5th node */
            PR_INFO("delete 5th node");
            list_del(&(nodes[5]->list_));

            /* iterate the array */
            PR_INFO("nodes[] iteration : ");
            for (i = 0; i < num_elements; i++) {
                PR_INFO("  nodes[%d] number_ : %d", i, nodes[i]->number_);
            }
            /* iterate the list */
            list_for_each_entry(node_iter, &(head_node->list_), list_) {
                PR_INFO("node number : %d", node_iter->number_);
            }
            /* remove all elements from list
               the _safe version allow for node removal from list while iterating list
            */
            list_for_each_safe(safe_iter_pos, safe_iter_temp, &(head_node->list_)) {
                linked_list_node = list_entry(safe_iter_pos, struct LinkedListNode, list_);
                list_del(&linked_list_node->list_);
            }
        }

        /* free nodes */
        PR_INFO("free nodes[] : ");
        for (i = 0; i < num_elements; i++) {
            PR_INFO("free node number : %d", nodes[i]->number_);
            kfree(nodes[i]);
        }
        PR_INFO("free node array");
        kfree(nodes);
        PR_INFO("free head node");
        kfree(head_node);

    } else {
        PR_INFO("head node kzalloc() failure");
    }
}
