# module name not a source file for multi-file builds
obj-m := ess_container_of.o
# one of these files contains the module MACRO credentials
ess_container_of-objs := ess_canonical_module.o container_of.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
