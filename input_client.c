/*
 ***********************************************

@Author:   zappen
@Mail:     zp935675863@gmail.com
@Date:     2024-04-16
@FileName: listen_kbd.c
 ***********************************************
*/

#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#include <pthread.h>

// 目标ip
#define SERVER_IP "192.168.217.129"
// 目标端口
#define SERVER_PORT 0x1024

// 根据实际修改
/* 键盘设备文件 */
#define KBD_DEV_PATH "/dev/input/event1" 
/* 鼠标设备文件 */
#define MOUSE_DEV_PATH "/dev/input/event6"

// 客户端socket句柄
static int client_sock_fd;
// 设备句柄
static int kbd_fd, mouse_fd;

// 打印事件函数，测试所用
void print_event(struct input_event ev) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("cur_time:%ld.%ld\t", tv.tv_sec, tv.tv_usec);
	printf("time:%ld.%ld\t type:%d\t code:%d\t value:%d\n\n", ev.time.tv_sec, ev.time.tv_usec, ev.type, ev.code, ev.value);
}

void init_socket() {
	int ret;
	struct sockaddr_in server_addr;
	/* 创建客户端套接字 */
	client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock_fd < 0) {
		perror("Failed to create socket");
		exit(EXIT_FAILURE);
	}
	
	/* 设置服务器地址信息 */
	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	server_addr.sin_port = htons(SERVER_PORT);
	
	/* 建立连接 */
	ret = connect(client_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perror("failed to connect to the server");
		exit(EXIT_FAILURE);
	}

}
void *listen_kbd_thread_func() {
	// 读取到的input设备数据
	struct input_event event;
	int ret;
	while (1) {
		// 从键盘设备文件里面读取键盘事件
		ret = read(kbd_fd, &event, sizeof(event));
		// 读取成功
		if (ret == sizeof(event)) {
			print_event(event);
			// 发送至另一设备
			ret = send(client_sock_fd, &event, sizeof(struct input_event), 0);
			if (ret < 0) {
				perror("Failed to send data to the server");
				exit(EXIT_FAILURE);
			}
		}
	}
	return NULL;

}
void *listen_mouse_thread_func() {
	// 读取到的input设备数据
	struct input_event event;
	int ret;
	while (1) {
		// 从鼠标设备文件里面读取键盘事件
		ret = read(mouse_fd, &event, sizeof(event));
		// 读取成功
		if (ret == sizeof(event)) {
			print_event(event);
			// 发送至另一设备
			ret = send(client_sock_fd, &event, sizeof(struct input_event), 0);
			if (ret < 0) {
				perror("Failed to send data to the server");
				exit(EXIT_FAILURE);
			}
		}
	}
	return NULL;

}
int main() {

	// 打开设备文件
	kbd_fd = open(KBD_DEV_PATH, O_RDONLY);
	if (kbd_fd <= 0) {
		printf("open %s device error!\n", KBD_DEV_PATH);
		return -1;
	}
	mouse_fd = open(MOUSE_DEV_PATH, O_RDONLY);
	if (mouse_fd <= 0) {
		printf("open %s device error!\n", MOUSE_DEV_PATH);
		return -1;
	}

	// 监听键盘设备文件并发送给服务端
	pthread_t listen_kbd_thread;	
	pthread_create(&listen_kbd_thread, NULL, listen_kbd_thread_func, NULL);
	// 监听鼠标设备文件并发送给服务端
	pthread_t listen_mouse_thread;
	pthread_create(&listen_mouse_thread, NULL, listen_mouse_thread_func, NULL);
	
	init_socket();
	pthread_join(listen_kbd_thread, NULL);
	pthread_join(listen_mouse_thread, NULL);
	// 关闭
	close(client_sock_fd);
	close(kbd_fd);
	close(mouse_fd);
	return 0;
}
