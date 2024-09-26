#ifndef _BASE_H
#define _BASE_H
#define IPLEN 20


struct servant_device {
	int flag;  // 标志从设备数组中该下标是否被占用
	int sit; // 0 - up, 1 - down, 2 - left, 3 - right
	char ip[IPLEN];
	int enable; // 0 - disable, 1 - enable;
	int status; // 0 - disconnect, 1 - connect
	int sock_fd;
	//int width;
	//int height;
};

struct devices_layout {
	int init_flag; // 1 -- initialize, 0 -- not initialize
	struct servant_device *up;
	struct servant_device *down;
	struct servant_device *left;
	struct servant_device *right;
};



#endif

