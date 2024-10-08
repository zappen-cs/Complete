/*
 ***********************************************

@Author:   zappen
@Mail:     zp935675863@gmail.com
@Date:     2024-07-16
@FileName: abs_mouse_device.c
 ***********************************************
*/


#include "abs_mouse_device.h"
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "debug_info.h"


int abs_mouse_fd = -1;
struct uinput_user_dev abs_mouse_dev;

void set_mouse_position(double px, double py) {
	//DEBUG_INFO("set\n");
	struct input_event mouse_ev;
	memset(&mouse_ev, 0, sizeof(struct input_event));

	gettimeofday(&mouse_ev.time, NULL);  
	mouse_ev.type = EV_ABS;
	mouse_ev.code = ABS_X;
	mouse_ev.value = px;
	if (write(abs_mouse_fd, &mouse_ev, sizeof(struct input_event)) < 0) {
		DEBUG_INFO("position error\n");
	}
 
    gettimeofday(&mouse_ev.time, NULL);  
	mouse_ev.type = EV_ABS;
	mouse_ev.code = ABS_Y;
	mouse_ev.value = py;
	if (write(abs_mouse_fd, &mouse_ev, sizeof(struct input_event)) < 0) {
		DEBUG_INFO("position error\n");
	}

    gettimeofday(&mouse_ev.time, NULL);  
	mouse_ev.type = EV_SYN;
	mouse_ev.code = SYN_REPORT;
	mouse_ev.value = 0;
	if (write(abs_mouse_fd, &mouse_ev, sizeof(struct input_event)) < 0) {
		DEBUG_INFO("position error\n");
	}
	DEBUG_INFO("set ok\n");

}

void create_abs_mouse() {
	int ret = 0;
	/* /dev/uinput - 该设备文件允许用户空间程序模拟输入设备，如鼠标、键盘 */
	abs_mouse_fd = open("/dev/uinput", O_RDWR | O_NDELAY);
	if(abs_mouse_fd < 0){
		DEBUG_INFO("error");
		return ;
	}
	
	/* 设置uinput设备, 设备名、版本号、总线类型 */
	memset(&abs_mouse_dev, 0, sizeof(struct uinput_user_dev));
	snprintf(abs_mouse_dev.name, UINPUT_MAX_NAME_SIZE, "abs_mouse_dev");
	abs_mouse_dev.id.version = 1;
	abs_mouse_dev.id.bustype = BUS_VIRTUAL;
	abs_mouse_dev.id.product = 0x2;
	abs_mouse_dev.id.vendor = 0x223;
	
	/* 设置设备支持的事件类型 —— 同步 */
	ioctl(abs_mouse_fd, UI_SET_EVBIT, EV_SYN);
	ioctl(abs_mouse_fd, UI_SET_EVBIT, EV_KEY);  
	ioctl(abs_mouse_fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(abs_mouse_fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(abs_mouse_fd, UI_SET_KEYBIT, BTN_MIDDLE);

	ioctl(abs_mouse_fd, UI_SET_EVBIT, EV_ABS);
	ioctl(abs_mouse_fd, UI_SET_ABSBIT, ABS_X);
	ioctl(abs_mouse_fd, UI_SET_ABSBIT, ABS_Y);
	abs_mouse_dev.absmin[ABS_X] = 0;
	abs_mouse_dev.absmax[ABS_X] = 1920;
	abs_mouse_dev.absfuzz[ABS_X] = 0;
	abs_mouse_dev.absflat[ABS_X] = 0;
	abs_mouse_dev.absmin[ABS_Y] = 0;
	abs_mouse_dev.absmax[ABS_Y] = 1080;
	abs_mouse_dev.absfuzz[ABS_Y] = 0;
	abs_mouse_dev.absflat[ABS_Y] = 0;

	/* 写入设备信息 */
	ret = write(abs_mouse_fd, &abs_mouse_dev, sizeof(struct uinput_user_dev));
	if(ret < 0){
		DEBUG_INFO("error");
		return ;
	}
	
	/* 创建虚拟输入设备 */
	ret = ioctl(abs_mouse_fd, UI_DEV_CREATE);
	if(ret < 0){
		DEBUG_INFO("error");
		close(abs_mouse_fd);
		return ;
	}

}
