/*
 ***********************************************

@Author:   zappen
@Mail:     zp935675863@gmail.com
@Date:     2023-06-10
@FileName: Server.c
 ***********************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>

// 端口号，需要与客户端保持一致
#define SERVER_PORT 0x1024

// vir_input_dev 是虚拟设备的一个抽象，需要对其设置以表明具体的虚拟设备
static struct uinput_user_dev vir_input_dev;
// 该虚拟设备的一个句柄
static int vir_input_fd;
 
// 服务端socket的句柄
static int server_sock_fd;
// 服务端用于处理客户端socket请求的句柄
static int client_sock_fd;

// 打印事件，测试使用
void print_event(struct input_event ev) {
	printf("type:%d\t code:%d\t value:%d\n\n", ev.type, ev.code, ev.value);
}

// 创建虚拟输入设备 -- 此程序创建的虚拟输入设备同时兼具鼠标和键盘的功能
static int creat_vir_input_device() {
	int ret = 0;
	/* /dev/uinput - 该设备文件允许用户空间程序模拟输入设备，如鼠标、键盘 */
	vir_input_fd = open("/dev/uinput", O_RDWR | O_NDELAY);
	if(vir_input_fd < 0){
		printf("error:%s:%d\n", __func__, __LINE__);
		return -1;//error process.
	}
	
	/* 设置uinput设备, 设备名、版本号、总线类型 */
	memset(&vir_input_dev, 0, sizeof(struct uinput_user_dev));
	snprintf(vir_input_dev.name, UINPUT_MAX_NAME_SIZE, "my-vir-input");
	vir_input_dev.id.version = 0x1;
	vir_input_dev.id.product = 0x2;
	vir_input_dev.id.bustype = BUS_VIRTUAL;
	
	/* 设置设备支持的事件类型 —— 同步、按键 */
	ioctl(vir_input_fd, UI_SET_EVBIT, EV_SYN);
	ioctl(vir_input_fd, UI_SET_EVBIT, EV_KEY);
	
	/* 设置设备支持所有的键盘按键 */
	for(int i = 0; i < 256; i++){
		ioctl(vir_input_fd, UI_SET_KEYBIT, i);
	}
	/* 设置设备支持所有的鼠标按键 */
	ioctl(vir_input_fd, UI_SET_KEYBIT, BTN_LEFT); // 支持鼠标左键
	ioctl(vir_input_fd, UI_SET_KEYBIT, BTN_RIGHT); // 支持鼠标右键
	ioctl(vir_input_fd, UI_SET_KEYBIT, BTN_MIDDLE); // 支持鼠标中键
	
	ioctl(vir_input_fd, UI_SET_EVBIT, EV_REL); // 相对事件
	ioctl(vir_input_fd, UI_SET_RELBIT, REL_X); // x轴相对坐标
	ioctl(vir_input_fd, UI_SET_RELBIT, REL_Y); // y轴相对坐标
											   //
	/* 写入设备信息 */
	ret = write(vir_input_fd, &vir_input_dev, sizeof(struct uinput_user_dev));
	if(ret < 0){
		printf("error:%s:%d\n", __func__, __LINE__);
		return ret;//error process.
	}
	
	/* 创建虚拟输入设备 */
	ret = ioctl(vir_input_fd, UI_DEV_CREATE);
	if(ret < 0){
		printf("error:%s:%d\n", __func__, __LINE__);
		close(vir_input_fd);
		return ret;//error process.
	}
}
/* 上报事件 */
static int report_event(unsigned int type, unsigned int code, unsigned int value)
{
	struct input_event ev;
	int ret;
	memset(&ev, 0, sizeof(struct input_event));
	
	/* 事件设置 */
	gettimeofday(&ev.time, NULL);
	ev.type = type;
	ev.code = code;
	ev.value = value;

	/* 上报 */
	ret = write(vir_input_fd, &ev, sizeof(struct input_event));
	if(ret < 0){
		printf("error:%s:%d\n", __func__, __LINE__);
		return ret;//error process.
	}
	return 0;
}

// 初始化socket
void init_socket() {
	int ret;
	/* 1.创建服务器socket，用于监听请求 */
	server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	/* 2.创建sockaddr_in结构体变量 */
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr)); // 清空server_addr
	server_addr.sin_family = AF_INET;	// 使用ipv4地址(32位)
	server_addr.sin_port = htons(SERVER_PORT); // 16位的端口号
	
	/* 3.与socket绑定 */
	ret = bind(server_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perror("Failed to bind socket");
		exit(EXIT_FAILURE);
	}
	/* 4.监听，等待用户发起请求 */
	ret = listen(server_sock_fd, 20);
	if (ret < 0) {
		perror("Failed to listen for connections");
		exit(EXIT_FAILURE);
	}

	/* 5.接收客户端请求 */
	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_len = sizeof(clnt_addr);
	client_sock_fd = accept(server_sock_fd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
	printf("start to listening--------------------\n");
}

int main() {
	int ret = 0;
	/* 1.创建虚拟输入设备 */
	ret = creat_vir_input_device();
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return -1;//error process.
	}
	struct input_event event;
	/* 2.初始化socket */
	init_socket();

	/* 3.开始等待接收客户端发来的事件并上报系统 */
	while (1) {
		/* 接收事件 */
		ret = recv(client_sock_fd, &event, sizeof(event), 0);
		if (ret <= 0) {
			close(server_sock_fd);
			close(client_sock_fd);
			perror("receive error");
			exit(EXIT_FAILURE);
		} else {
			print_event(event);
			/* 将事件上报至虚拟设备文件中 */
			report_event(event.type, event.code, event.value);
		}
	}
	return 0;
}

 
