
# module name not a source file for multi-file builds
obj-m := ess_oled.o
# one of these files contains the module MACRO credentials
ess_oled-objs := ess_canonical_module.o i2c_oled.o
# CLAGS_ess_oled.o := -DDEBUG

# # all:
# # 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# # clean:
# # 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
