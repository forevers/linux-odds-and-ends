# module name not a source file for multi-file builds
obj-m := ess_wait_queues.o
# one of these files contains the module MACRO credentials
ess_wait_queues-objs := ess_canonical_module.o wait_queues.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
