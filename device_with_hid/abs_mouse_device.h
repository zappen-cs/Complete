#ifndef _ABS_MOUSE_DEVICE_H
#define _ABS_MOUSE_DEVICE_H




// 创建一个绝对位置的鼠标
extern struct uinput_user_dev abs_mouse_dev;
// 绝对位置鼠标的一个句柄
extern int abs_mouse_fd;
// 鼠标全局位置
extern double global_x, global_y;
// 创建一个支持绝对坐标的鼠标
void create_abs_mouse();
// 设置鼠标位置
void set_mouse_position(double px, double py);

#endif // _ABS_MOUSE_DEVICE_H
