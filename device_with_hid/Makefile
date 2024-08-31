CC = gcc
CFLAGS = -Wall
INCLUDES = -I./
LDFLAGS = -lpthread -linput -ludev

SRCS = abs_mouse_device.c \
	   cJSON.c \
	   crypto.c\
	   get_target_dev_info.c \
	   detect_mouse_kbd_event.c \
	   input_device_shared.c \


TARGET = input_device_shared

all: $(TARGET)
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) $(LDFLAGS)

run:
	sudo -E ./$(TARGET)
clean:
	rm -f $(TARGET)
