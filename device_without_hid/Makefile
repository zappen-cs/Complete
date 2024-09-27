CC = gcc
CFLAGS = -Wall
INCLUDES = -I./
LDFLAGS = -lpthread -linput -ludev -lwayland-client

SRCS = abs_mouse_device.c \
	   crypto.c \
	   get_resolution.c \
	   input_server.c \
	   
	   
	  
	 

TARGET = input_device_server.so

all: $(TARGET)
$(TARGET): $(SRCS)
	$(CC) --shared -fpic $(CFLAGS) $(INCLUDES) $(SRCS) -o ../$(TARGET) $(LDFLAGS)

run:
	sudo -E ./$(TARGET)
clean:
	rm -f $(TARGET)
