# module name not a source file for multi-file builds
obj-m := ess_memory.o
# one of these files contains the module MACRO credentials
ess_memory-objs := ess_canonical_module.o memory.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

