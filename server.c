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

void print_event(struct input_event t) {
	printf("type:%d\t code:%d\t value:%d\n\n", t.type, t.code, t.value);
}

static struct uinput_user_dev uinput_dev;
static int uinput_fd;
 
int creat_user_uinput(void);
int report_key(unsigned int type, unsigned int keycode, unsigned int value);

int main() {
	int ret = 0;
	/* 创建虚拟设备 */
	ret = creat_user_uinput();
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return -1;//error process.
	}

	struct input_event event;
	/* 1.创建服务器socket，用于监听请求 */
	int	server_sock;

	/* 2.创建sockaddr_in结构体变量 */
	struct sockaddr_in server_addr;
	
	
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	memset(&server_addr, 0, sizeof(server_addr)); // 清空server_addr
								  
	server_addr.sin_family = AF_INET;	// 使用ipv4地址(32位)
	//server_addr.sin_addr.s_addr = inet_addr("192.168.12.141"); // 本机ip
	server_addr.sin_port = htons(SERVER_PORT); // 16位的端口号
	
	/* 3.与socket绑定 */
	ret = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perror("Failed to bind socket");
		exit(EXIT_FAILURE);
	}
	/* 4.监听，等待用户发起请求 */
	ret = listen(server_sock, 20);
	if (ret < 0) {
		perror("Failed to listen for connections");
		exit(EXIT_FAILURE);
	}

	/* 5.接收客户端请求 */
	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_len = sizeof(clnt_addr);
	int client_sock = accept(server_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_len);

	/* 6.开始通信 */
	while (1) {
		/* 接收事件 */
		ret = recv(client_sock, &event, sizeof(event), 0);
		if (ret < 0) {
			perror("receive error");
			exit(EXIT_FAILURE);
		} else {
	//		print_event(event);
			/* 将事件上报至虚拟设备文件中 */
			report_key(event.type, event.code, event.value);
		}
		//memcpy(send_buf, recv_buf, sizeof(recv_buf));
		//send(client_sock, send_buf, sizeof(send_buf), 0);
	}
	return 0;
}

int creat_user_uinput(void)
{
	int i;
	int ret = 0;
 
	/* /dev/uinput - 该设备文件允许用户空间程序模拟输入设备，如鼠标、键盘 */
	uinput_fd = open("/dev/uinput", O_RDWR | O_NDELAY);
	if(uinput_fd < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return -1;//error process.
	}
	
	/* 设置uinput设备, 设备名、版本号、总线类型 */
	memset(&uinput_dev, 0, sizeof(struct uinput_user_dev));
	snprintf(uinput_dev.name, UINPUT_MAX_NAME_SIZE, "my-vir-kbd");
	uinput_dev.id.version = 1;
	uinput_dev.id.bustype = BUS_VIRTUAL;
	
	/* 设置设备支持的事件类型 —— 同步、按键 */
	ioctl(uinput_fd, UI_SET_EVBIT, EV_SYN);
	ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
	ioctl(uinput_fd, UI_SET_EVBIT, EV_MSC);

	
	/* 设置设备支持所有的键盘按键 */
	for(i = 0; i < 256; i++){
		ioctl(uinput_fd, UI_SET_KEYBIT, i);
	}
	
	/* 写入设备信息 */
	ret = write(uinput_fd, &uinput_dev, sizeof(struct uinput_user_dev));
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return ret;//error process.
	}
	
	/* 创建键盘设备 */
	ret = ioctl(uinput_fd, UI_DEV_CREATE);
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		close(uinput_fd);
		return ret;//error process.
	}
}
 
/* 上报事件 */
int report_key(unsigned int type, unsigned int keycode, unsigned int value)
{
	struct input_event key_event;
	int ret;
	memset(&key_event, 0, sizeof(struct input_event));
	
	/* 事件设置 */
	gettimeofday(&key_event.time, NULL);
	key_event.type = type;
	key_event.code = keycode;
	key_event.value = value;

	/* 上报 */
	ret = write(uinput_fd, &key_event, sizeof(struct input_event));
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return ret;//error process.
	}
	return 0;
}
