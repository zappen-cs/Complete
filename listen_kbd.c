#hhhhhhhhhhhhhhh
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


// 目标ip
#define SERVER_IP "192.168.12.105"
// 目标端口
#define SERVER_PORT 0x1024

// 根据实际修改
#define DEV_PATH "/dev/input/event4"

// 打印事件函数，测试所用
void print_event(struct input_event t) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("cur_time:%ld.%ld\n", tv.tv_sec, tv.tv_usec);
	printf("time:%ld.%ld\t type:%d\t code:%d\t value:%d\n\n", t.time.tv_sec, t.time.tv_usec, t.type, t.code, t.value);
}

int main() {
	// 文件标识
	int kbd_fd;
	//char ret[2];
	// 读取到的input设备数据
	struct input_event t;

	// 打开设备文件
	kbd_fd = open(DEV_PATH, O_RDONLY);
	if (kbd_fd <= 0) {
		printf("open %s device error!\n", DEV_PATH);
		return -1;
	}
	int ret;
	int client_sock;
	struct sockaddr_in server_addr;
	/* 创建客户端套接字 */
	client_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock < 0) {
		perror("Failed to create socket");
		exit(EXIT_FAILURE);
	}
	
	/* 设置服务器地址信息 */
	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	server_addr.sin_port = htons(SERVER_PORT);
	
	/* 建立连接 */
	ret = connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perror("failed to connect to the server");
		exit(EXIT_FAILURE);
	}

	while (1) {
		// 从键盘设备文件里面读取键盘事件
		ret = read(kbd_fd, &t, sizeof(t));
		// 读取成功
		if (ret == sizeof(t)) {
			// 发送至另一设备
			ret = send(client_sock, &t, sizeof(struct input_event), 0);
			if (ret < 0) {
				perror("Failed to send data to the server");
				exit(EXIT_FAILURE);
			}
//			print_event(t);
		} else {
			break;
		}
	}
	
	// 关闭
	close(client_sock);
	close(kbd_fd);
	return 0;
}
