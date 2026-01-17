obj-m += test_module.o

PWD := $(shell pwd)

KERNEL_VERSION := $(shell uname -r)
KERNEL_BUILD := /lib/modules/$(KERNEL_VERSION)/build

ifeq ($(wildcard $(KERNEL_BUILD)),)
    AVAILABLE_BUILD := $(shell ls -td /lib/modules/*/build 2>/dev/null | head -1)
    ifneq ($(AVAILABLE_BUILD),)
        KERNEL_BUILD := $(AVAILABLE_BUILD)
        $(warning WARNING: Using kernel headers from $(KERNEL_BUILD) instead of $(KERNEL_VERSION))
        $(warning This may cause compatibility issues. Consider installing matching headers.)
    endif
endif

KDIR := $(KERNEL_BUILD)

FILENAME ?= /var/tmp/test_module/kernel_log_$(shell date +%s).txt
TIMER_PERIOD ?= 2

all:
	@if [ ! -d "$(KDIR)" ]; then \
		echo "ERROR: Kernel headers not found!"; \
		echo "Current kernel version: $(KERNEL_VERSION)"; \
		echo ""; \
		echo "Please install kernel headers:"; \
		echo "  Arch Linux: sudo pacman -S linux-headers"; \
		echo "  Debian/Ubuntu: sudo apt-get install linux-headers-$(KERNEL_VERSION)"; \
		echo "  Fedora: sudo dnf install kernel-devel"; \
		echo ""; \
		echo "Available kernel builds:"; \
		ls -d /lib/modules/*/build 2>/dev/null || echo "  (none found)"; \
		exit 1; \
	fi
	@echo "Using kernel headers from: $(KDIR)"
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order

load:
	@if [ ! -f test_module.ko ]; then \
		echo "ERROR: test_module.ko not found. Run 'make all' first."; \
		exit 1; \
	fi
	@echo "Creating log directory..."
	@sudo mkdir -p /var/tmp/test_module
	@sudo chmod 777 /var/tmp/test_module
	@echo "Creating log file..."
	@LOG_DIR=$$(dirname "$(FILENAME)"); \
	if [ ! -d "$$LOG_DIR" ]; then \
		sudo mkdir -p "$$LOG_DIR"; \
		sudo chmod 777 "$$LOG_DIR"; \
	fi; \
	sudo touch "$(FILENAME)"; \
	sudo chmod 666 "$(FILENAME)"; \
	echo "Log file: $(FILENAME)"; \
	if lsmod | grep -q test_module; then \
		echo "Module already loaded, unloading first..."; \
		sudo rmmod test_module 2>/dev/null || true; \
		sleep 1; \
	fi; \
	echo "Loading module with parameters..."; \
	echo "  filename=$(FILENAME)"; \
	echo "  timer_period=$(TIMER_PERIOD)"; \
	if sudo insmod test_module.ko filename="$(FILENAME)" timer_period=$(TIMER_PERIOD) --force 2>/dev/null; then \
		echo "Module loaded successfully"; \
		echo "Log file: $(FILENAME)"; \
		echo "Timer period: $(TIMER_PERIOD) seconds"; \
	else \
		echo "ERROR: Failed to load module"; \
		exit 1; \
	fi

unload:
	@if lsmod | grep -q test_module; then \
		echo "Unloading module..."; \
		sudo rmmod test_module && echo "Module unloaded successfully" || echo "ERROR: Failed to unload module"; \
	else \
		echo "Module is not loaded"; \
	fi

run: all load
	@echo ""
	@echo "Module is running. Use 'make unload' to unload it."
	@echo "To view logs: dmesg | grep test_module"
	@echo "Log file: $(FILENAME)"
	@echo "Timer period: $(TIMER_PERIOD) seconds"

help:
	@echo "Available targets:"
	@echo "  make all                    - Compile the kernel module"
	@echo "  make clean                  - Clean build artifacts"
	@echo "  make load                   - Load module with default parameters"
	@echo "  make load FILENAME=...      - Load module with custom filename"
	@echo "  make load TIMER_PERIOD=N    - Load module with custom timer period"
	@echo "  make load FILENAME=... TIMER_PERIOD=N  - Load with both parameters"
	@echo "  make unload                 - Unload the module"
	@echo "  make run                    - Compile and load with default parameters"
	@echo "  make run FILENAME=... TIMER_PERIOD=N  - Compile and load with custom parameters"
	@echo ""
	@echo "Examples:"
	@echo "  make load FILENAME=/var/tmp/test_module/my_log.txt TIMER_PERIOD=5"
	@echo "  make run FILENAME=/var/tmp/test_module/demo.txt TIMER_PERIOD=1"

.PHONY: all clean load unload run help
