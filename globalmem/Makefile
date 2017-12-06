KVERS = $(shell uname -r)

# Kernel moudles
obj-m += globalmem.o

# Specify flags for the module compilation.
#EXTRA_CFLASS=-g -O0


build: kernel_modules

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules
clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
