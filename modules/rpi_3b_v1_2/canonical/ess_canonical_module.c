/* 
    build syntax :
        cross :
            $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C <path to cross compiled kernel>/linux M=$(pwd) modules
        native :
            $ make -c /lib/modules/$(uname -r)/build M=$(pwd) modules

    load syntax :
        sudo insmod ess_canonical.ko
        sudo insmod ess_canonical.ko module_string="test" module_int_val=5 module_intarray=5,10
        sudo rmmod
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

MODULE_AUTHOR("Developer Name <developer_email>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("a canonical driver template");
MODULE_VERSION("0.1");

#include "gpio_irq.h"
#include "container_of.h"


/* default module parameters */
static char* module_string = "default module string";
static int module_int_val = 2;
static int module_intarray[2] = {0, 1};
module_param(module_string, charp, S_IRUGO); // read
MODULE_PARM_DESC(module_string, "parameter of string type");
module_param(module_int_val, int, S_IRUGO); // read
MODULE_PARM_DESC(module_int_val, "parameter of int type");
module_param_array(module_intarray, int, NULL, S_IWUSR|S_IRUSR); // read/write
MODULE_PARM_DESC(module_intarray, "parameter of int array type");


/* log module parameters */
static void log_parameters(void)
{
    pr_info("module_string: %s\n", module_string);
    pr_info("module_int_val: %d\n", module_int_val);
    pr_info("module_intarray: %d, %d\n", module_intarray[0], module_intarray[1]);

    return;
}


/* module load 
    for built-ins this code is placed in a mem section which is freed after init is complete
*/
static int __init
canonical_init(void)
{
    pr_info("canonical_init() entry\n");

    log_parameters();

    container_demo();

    gpio_irq_demo_init();

    pr_info("canonical_init() exit\n");
    return 0;
}


/* module unload for non-built ins */
static void __exit
canonical_exit(void)
{
    pr_info("canonical_exit() entry\n");

    gpio_irq_exit();

    pr_info("canonical_exit() exit\n");
}


module_init(canonical_init);
module_exit(canonical_exit);
