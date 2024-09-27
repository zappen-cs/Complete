CC = gcc
CFLAGS = -Wall
INCLUDES = -I./
LDFLAGS = -lpthread -linput -ludev -lwayland-client

SRCS = abs_mouse_device.c \
	   cJSON.c \
	   crypto.c\
	   get_target_dev_info.c \
	   detect_mouse_kbd_event.c \
	   input_device_shared.c \
	   get_resolution.c \


TARGET = input_device_shared.so

all: $(TARGET)
$(TARGET): $(SRCS)
	$(CC) --shared -fpic $(CFLAGS) $(INCLUDES) $(SRCS) -o ../$(TARGET) $(LDFLAGS)

run:
	sudo -E ./$(TARGET)
clean:
	rm -f $(TARGET)
