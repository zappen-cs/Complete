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

char** detect_all_mouse(int *count);
char** detect_all_kbd(int *count);

char* detect_mouse();
char* detect_kbd();