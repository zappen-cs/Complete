#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <linux/input.h>
#include <errno.h>

#define MAX_LINE_LENGTH 1024
#define INPUT_DIR "/dev/input/"


int is_mouse_device(const char *device) {
    // Filter by events
    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 0;
    }

    unsigned long ev_bits[EV_MAX/8 + 1] = {0};
    unsigned long key_bits[KEY_MAX/8 + 1] = {0};

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        perror("Failed to get event bits");
        close(fd);
        return 0;
    }

    if (!(ev_bits[EV_KEY / 8] & (1 << (EV_KEY % 8)))) {
        close(fd);
        return 0;
    }

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        perror("Failed to get key bits");
        close(fd);
        return 0;
    }

    if (ev_bits[EV_REL / 8] & (1 << (EV_REL % 8)) &&
        ev_bits[EV_MSC / 8] & (1 << (EV_MSC % 8))
        ) {
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}


int is_kbd_device(const char *device) {
    // Filter by events
    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 0;
    }

    unsigned long ev_bits[EV_MAX/8 + 1] = {0};
    unsigned long key_bits[KEY_MAX/8 + 1] = {0};

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        perror("Failed to get event bits");
        close(fd);
        return 0;
    }

    if (!(ev_bits[EV_KEY / 8] & (1 << (EV_KEY % 8)))) {
        close(fd);
        return 0;
    }

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        perror("Failed to get key bits");
        close(fd);
        return 0;
    }

    // No EV_REL when identifying keyboard

    if(!(key_bits[KEY_BACKSPACE / 8] & (1 << (KEY_BACKSPACE % 8)))){
        close(fd);
        return 0;
    }

    if (!(ev_bits[EV_REL / 8] & (1 << (EV_REL % 8)))&&
        ev_bits[EV_MSC / 8] & (1 << (EV_MSC % 8))
        ) {
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}



int is_mouse(const char *device_info) {
    // Check if the line contains Mouse"
    // Filter VITRUAL devices
    return (strstr(device_info, "Mouse") != NULL && strstr(device_info, "Virtual") == NULL);
}


int is_kbd(const char *device_info) {
    // Check if the line contains "USB" and "Mouse"
    return (strstr(device_info, "keyboard") != NULL || strstr(device_info, "Keyboard") != NULL && strstr(device_info, "Virtual") == NULL) ;
}


char* get_event_file(const char *device_info){
    return strstr(device_info, "event");
}


char** detect_mouse(int *count) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char path[PATH_MAX];
    char **mouse_devices = NULL;
    *count = 0;

    fp = fopen("/proc/bus/input/devices", "r");
    if (fp == NULL) {
        perror("Failed to open devices file");
        return mouse_devices;
    }

    // If current device is mouse
    int flag = 0;
    int is_usb = 0;
    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        // Check if the device is a usb device
        if (line[0] == 'I'){
            is_usb = strstr(line, "Bus=0003") != NULL;
        }
        // Check if the device is mouse
        if (line[0] == 'N'){
            flag = is_mouse(line) && is_usb;
        }
        // Find eventX in handler
        if (line[0] == 'H' && flag) {
            flag = 0;
            is_usb = 0;
            snprintf(path, sizeof(path), "%s%s", INPUT_DIR, get_event_file(line));
            char *end = strchr(path, '\n');
            *(end-1) = '\0';
            if(!is_mouse_device(path)){
                continue;
            }
            mouse_devices = realloc(mouse_devices, (*count + 1) * sizeof(char*));
            mouse_devices[*count] = strdup(path);
            
            // Test
            // printf("mouse device found:\n%s", line);
            (*count)++;
        }
    }

    fclose(fp);

    if (count == 0) {
        printf("?\n");
    }

    return mouse_devices;
}


char** detect_kbd(int *count) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char path[PATH_MAX];
    char **kbd_devices = NULL;
    *count = 0;

    fp = fopen("/proc/bus/input/devices", "r");
    if (fp == NULL) {
        perror("Failed to open devices file");
        return kbd_devices;
    }

    // If current device is kbd
    int flag = 0;
    int is_usb = 0;

    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        // Check if the device is a usb device
        if (line[0] == 'I'){
            is_usb = strstr(line, "Bus=0003") != NULL;
        }
        // Check if the device is kbd
        if (line[0] == 'N'){
            flag = is_kbd(line) && is_usb;
        }
        // Find eventX in handler
        if (line[0] == 'H' && flag) {
            flag = 0;
            is_usb = 0;
            snprintf(path, sizeof(path), "%s%s", INPUT_DIR, get_event_file(line));
            char *end = strchr(path, '\n');
            *(end-1) = '\0';
            if(!is_kbd_device(path)){
                continue;
            }
            kbd_devices = realloc(kbd_devices, (*count + 1) * sizeof(char*));
            kbd_devices[*count] = strdup(path);
            
            // Test
            // printf("kbd device found:\n%s", line);
            (*count)++;
        }
    }

    fclose(fp);

    if (count == 0) {
        printf("?\n");
    }

    return kbd_devices;
}


int main(){
    int cnt_mouse = 0;
    int cnt_kbd = 0;
    char **mouse_devices = detect_mouse(&cnt_mouse);
    char **kbd_devices = detect_kbd(&cnt_kbd);

    for (int i = 0; i < cnt_mouse; i++) {
        printf("%s\n", mouse_devices[i]);
        free(mouse_devices[i]);
    }
    printf("-------------------\n");
    for (int i = 0; i < cnt_kbd; i++) {
        printf("%s\n", kbd_devices[i]);
        free(kbd_devices[i]);
    }
}
