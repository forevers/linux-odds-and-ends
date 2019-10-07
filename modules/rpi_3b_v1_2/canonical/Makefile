# module name not a source file for multi-file builds
obj-m := ess_canonical.o
# one of these files contains the module MACRO credentials
ess_canonical-objs := ess_canonical_module.o container_of.o gpio_irq.o circ_buffer_mod2_elements.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
