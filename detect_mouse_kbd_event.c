#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_LINE_LENGTH 1024
#define INPUT_DIR "/dev/input/"

int is_mouse(const char *device_info) {
    // Check if the line contains "USB" and "Mouse"
    return (strstr(device_info, "USB") != NULL && strstr(device_info, "Mouse") != NULL);
}


int is_kbd(const char *device_info) {
    // Check if the line contains "USB" and "Mouse"
    return (strstr(device_info, "keyboard") != NULL);
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

    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        // Check if the device is mouse
        if (line[0] == 'N'){
            flag = is_mouse(line);
        }
        // Find eventX in handler
        if (line[0] == 'H' && flag) {
            flag = 0;
            snprintf(path, sizeof(path), "%s%s", INPUT_DIR, get_event_file(line));
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

    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        // Check if the device is kbd
        if (line[0] == 'N'){
            flag = is_kbd(line);
        }
        // Find eventX in handler
        if (line[0] == 'H' && flag) {
            flag = 0;
            snprintf(path, sizeof(path), "%s%s", INPUT_DIR, get_event_file(line));
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
        printf("%s", mouse_devices[i]);
        free(mouse_devices[i]);
    }
    printf("-------------------");
    for (int i = 0; i < cnt_kbd; i++) {
        printf("%s", kbd_devices[i]);
        free(kbd_devices[i]);
    }
}
