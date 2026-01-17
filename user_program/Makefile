CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = set_params
SOURCE = set_params.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

set-period:
	@if [ -z "$(PERIOD)" ]; then \
		echo "Usage: make set-period PERIOD=N"; \
		echo "Example: make set-period PERIOD=1"; \
		exit 1; \
	fi
	@sudo ./$(TARGET) -p $(PERIOD)

set-filename:
	@if [ -z "$(FILENAME)" ]; then \
		echo "Usage: make set-filename FILENAME=/path/to/file"; \
		echo "Example: make set-filename FILENAME=/var/tmp/test_module/log.txt"; \
		exit 1; \
	fi
	@sudo ./$(TARGET) -f $(FILENAME)

set-params:
	@if [ -z "$(FILENAME)" ] && [ -z "$(PERIOD)" ]; then \
		echo "Usage: make set-params [FILENAME=/path/to/file] [PERIOD=N]"; \
		echo "Example: make set-params FILENAME=/var/tmp/test_module/log.txt PERIOD=5"; \
		echo "         make set-params FILENAME=/var/tmp/test_module/log.txt"; \
		echo "         make set-params PERIOD=10"; \
		exit 1; \
	fi
	@ARGS=""; \
	if [ -n "$(FILENAME)" ]; then ARGS="$$ARGS -f $(FILENAME)"; fi; \
	if [ -n "$(PERIOD)" ]; then ARGS="$$ARGS -p $(PERIOD)"; fi; \
	sudo ./$(TARGET) $$ARGS

.PHONY: all clean set-period set-filename set-params

