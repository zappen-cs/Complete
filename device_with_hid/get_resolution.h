#ifndef _GET_RESOLUTION_H
#define _GET_RESOLUTION_H

struct display_info {
    int width;
    int height;
};

int get_resolution(int *width, int *height);

#endif // _GET_RESOLUTION_H
