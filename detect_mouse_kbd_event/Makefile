CC = gcc
CFLAGS = -Wall
INCLUDES = -I./
LDFLAGS =

SRCS = detect_mouse_kbd_event.c


TARGET = detect_mouse_kbd_event.so

all: $(TARGET)
$(TARGET): $(SRCS)
	$(CC) --shared -fpic $(CFLAGS) $(INCLUDES) $(SRCS) -o ../$(TARGET) $(LDFLAGS)

run:
	sudo -E ./$(TARGET)
clean:
	rm -f $(TARGET)
