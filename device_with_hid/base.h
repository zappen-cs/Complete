#ifndef _BASE_H
#define _BASE_H
#define IPLEN 20


struct servant_device {
	int flag;  // 标志从设备数组中该下标是否被占用
	char ip[IPLEN];
	int sock_fd;
};

struct devices_layout {
	struct servant_device *up;
	struct servant_device *down;
	struct servant_device *left;
	struct servant_device *right;
};


#endif

