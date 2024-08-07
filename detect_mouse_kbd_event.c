#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
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

char* to_lower_str(const char* device_info){
    char* ret_str = calloc(strlen(device_info), sizeof(char));
    for(int i = 0; i < strlen(device_info); ++i){
        ret_str[i] = tolower(device_info[i]);
    }
    return ret_str;
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

int is_virtual(const char *device_info) {
    char* device_info_low = to_lower_str(device_info);
    int flag = strstr(device_info_low, "virtual") != NULL;
    free(device_info_low);
    return flag;
}

int is_mouse(const char *device_info) {
    // Check if the line contains mouse"
    char* device_info_low = to_lower_str(device_info);
    int flag = strstr(device_info, "mouse") != NULL;
    free(device_info_low);
    return flag;
}


int is_kbd(const char *device_info) {
    // Check if the line contains "keyboard" or "kbd"
    char* device_info_low = to_lower_str(device_info);
    int flag = strstr(device_info, "keyboard") != NULL || strstr(device_info, "kbd") != NULL;
    free(device_info_low);
    return flag;
}


char* get_event_file(const char *device_info){
    return strstr(device_info, "event");
}


char** detect_all_mouse(int *count) {
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
    int virtual = 0;
    int usb = 0;
    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        // Check if the device is a usb device
        if (line[0] == 'I'){
            usb = strstr(line, "Bus=0003") != NULL;
        }
        // Check if the device is mouse
        if (line[0] == 'N'){
            flag = is_mouse(line);
            virtual = is_virtual(line);
        }
        // Find eventX in handler
        if (line[0] == 'H' && usb && !virtual && (flag || is_mouse(line))) {
            flag = 0;
            usb = 0;
            virtual = 0;
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
        if (line[0] == 'B'){
            usb = 0;
            virtual = 0;
            flag = 0;
        }
    }

    fclose(fp);

    if (count == 0) {
        printf("?\n");
    }

    return mouse_devices;
}


char** detect_all_kbd(int *count) {
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
    int usb = 0;
    int virtual = 0;

    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        // Check if the device is a usb device
        if (line[0] == 'I'){
            usb = strstr(line, "Bus=0003") != NULL;
        }
        // Check if the device is kbd
        if (line[0] == 'N'){
            flag = is_kbd(line);
            virtual = is_virtual(line);
        }
        // Find eventX in handler
        if (line[0] == 'H' && usb && !virtual && (flag || is_kbd(line))) {
            flag = 0;
            usb = 0;
            virtual = 0;
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
        if (line[0] == 'B'){
            usb = 0;
            virtual = 0;
            flag = 0;
        }
    }

    fclose(fp);

    if (count == 0) {
        printf("?\n");
    }

    return kbd_devices;
}

char* detect_kbd(){
    // 
    int cnt = 0;
    char**  ret = detect_all_kbd(&cnt);
    if (cnt == 0){
        printf("no usb non-virtual kbd found\n");
        return "";
    }
    return ret[0];
}

char* detect_mouse(){
    int cnt = 0;
    char**  ret = detect_all_mouse(&cnt);
    if (cnt == 0){
        printf("no usb non-virtual mouse found\n");
        return "";
    }
    return ret[0];
}

// int main(){
//     int cnt_mouse = 0;
//     int cnt_kbd = 0;
//     char **mouse_devices = detect_all_mouse(&cnt_mouse);
//     char **kbd_devices = detect_all_kbd(&cnt_kbd);

//     for (int i = 0; i < cnt_mouse; i++) {
//         printf("%s\n", mouse_devices[i]);
//         free(mouse_devices[i]);
//     }
//     printf("-------------------\n");
//     for (int i = 0; i < cnt_kbd; i++) {
//         printf("%s\n", kbd_devices[i]);
//         free(kbd_devices[i]);
//     }

//     printf("%s\n", detect_mouse());
//     printf("-------------\n");
//     printf("%s\n", detect_kbd());
//     return 0;
// }
