# module name not a source file for multi-file builds
obj-m := ess_linked_list.o
# one of these files contains the module MACRO credentials
ess_linked_list-objs := ess_canonical_module.o linked_list.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
