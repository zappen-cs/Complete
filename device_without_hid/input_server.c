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
#include "debug_info.h"
#include <pthread.h>



#include <sys/time.h>

#include <libinput.h>
#include <libudev.h>
#include <errno.h>
#include <sys/wait.h>

#include "abs_mouse_device.h"
#include "debug_info.h"
#include "crypto.h"

#define SERVER_PORT 0x1024


#define SET_MOUSE_POSITION 0x1234560
#define KBD_EVENT 0x1234561
#define MOUSE_EVENT 0x1234562

#define ENABLE_COLIPBOARD 1
#if ENABLE_COLIPBOARD
#define CLIPBOARD_EVENT 0x1234563

#define BUFFER_SIZE 1024
uint8_t recv_buf[BUFFER_SIZE];
uint8_t send_buf[BUFFER_SIZE];
uint8_t clipboard_content[BUFFER_SIZE];
#endif

typedef struct {
	double x;
	double y;
} mouse_positon;

struct mouse_kbd_event {
	int type;
	union {
		struct input_event ev;
		mouse_positon pos;
#if ENABLE_COLIPBOARD
		uint8_t data[BUFFER_SIZE];
#endif
	};
};

static struct uinput_user_dev vir_input_dev;
static int vir_input_fd;
 
static int server_sock_fd;
static int client_sock_fd;

#if 0
void print_event(struct input_event ev) {
	printf("type:%d\t code:%d\t value:%d\n\n", ev.type, ev.code, ev.value);
}
#endif

double global_x = 960, global_y = 540;

const char key[] = "123";//the decrypt key

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);
    if (fd < 0)
        fprintf(stderr, "Failed to open %s (%s)\n", path, strerror(errno));
    return fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

static struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void handle_event(struct libinput_event *event) {
    if (libinput_event_get_type(event) == LIBINPUT_EVENT_POINTER_MOTION) {
        struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event(event);
        //double dx_unacce = libinput_event_pointer_get_dx_unaccelerated(pointer_event);
        //double dy_unacce = libinput_event_pointer_get_dy_unaccelerated(pointer_event);
        double dx_acce = libinput_event_pointer_get_dx(pointer_event);
        double dy_acce = libinput_event_pointer_get_dy(pointer_event);

		//global_x += dx_unacce * (1 + pointer_speed), global_y += dy_unacce * (1 + pointer_speed);
		global_x += dx_acce, global_y += dy_acce;

		/* we assume we have two device, dev1 and dev2, 
		 * their screen is 1920x1080 and their position follow: 
		 *         ____________________     ____________________
		 *  	  |					   |   |					|
		 *		  |                    |   |                    |
		 *        |        dev1        |   |        dev2        |
		 *        |                    |   |                    |
		 *        |____________________|   |____________________|
		 */
		if (global_x <= 0) {
			DEBUG_INFO("Mouse reached the left boundary");
			global_x = 0;
			struct mouse_kbd_event mke;
			memset(&mke, 0, sizeof(mke));
			mke.type = SET_MOUSE_POSITION;
			mke.pos.x = 1920;
			mke.pos.y = global_y;
			send(client_sock_fd, &mke, sizeof(mke), 0);
		}
		if (global_x >= 1920) {
			DEBUG_INFO("Mouse reached the right boundary");
			global_x = 1920;
		}
		if (global_y <= 0) {
			DEBUG_INFO("Mouse reached the top boundary");
			global_y = 0;
		}
		if (global_y >= 1080) {
			DEBUG_INFO("Mouse reached the bottom boundary");
			global_y = 1080;
		}
    }
}
#if ENABLE_COLIPBOARD
void copy_from_clipboard() {
	FILE *wl_paste_ptr = popen("/usr/bin/wl-paste", "r");
	if (wl_paste_ptr == NULL) {
		printf("Failed to open wl-paste");
		return ;
	}
	fgets(clipboard_content, BUFFER_SIZE, wl_paste_ptr);
	clipboard_content[strcspn(clipboard_content, "\n")] = '\0';
	pclose(wl_paste_ptr);
}
void *listen_clipboard_thread_func() {
	int ret;
	while (1) {
		sleep(1);
		copy_from_clipboard();
		if (strcmp(clipboard_content, send_buf) == 0)  // no changed
			continue;
		// user has executed wl-copy
		strcpy(send_buf, clipboard_content);
		struct mouse_kbd_event mke;
		mke.type = CLIPBOARD_EVENT;
		strcpy(mke.data, clipboard_content);
		//encrypt the message
		printf("The original message:%s\n",mke.data);
		xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
		printf("The jiami message:%s\n",mke.data);
		//
		ret = send(client_sock_fd, &mke, sizeof(mke), 0);
		if (ret < 0) {
			printf("Failed to send data to client\n");
			break;
		}
	}
	return NULL;
}
#endif
void *watch_mouse_thread_func() {
    struct udev *udev = udev_new();
    struct libinput *li = libinput_udev_create_context(&interface, NULL, udev);
    libinput_udev_assign_seat(li, "seat0");

    struct libinput_event *event;
	while (1) {
		libinput_dispatch(li);
		while ((event = libinput_get_event(li)) != NULL) {
			// 处理事件
			handle_event(event);
			libinput_event_destroy(event);
		}
	}

    libinput_unref(li);
    udev_unref(udev);
	return NULL;
}

void *set_mouse_positon_func() {
	/* set mouse's absolute position in screen*/
	create_abs_mouse();
	usleep(100000);
	global_x = 0, global_y = 540;
	set_mouse_position(global_x, global_y);
	//ioctl(abs_mouse_fd, UI_DEV_DESTROY);
	//close(abs_mouse_fd);
	return NULL;
}

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
	ioctl(vir_input_fd, UI_SET_RELBIT, REL_WHEEL);
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
	return 0;
}

#if ENABLE_COLIPBOARD
void copy_to_clipboard(const char *content) {
	char command[1024];
	FILE *wl_copy_ptr = NULL;
	snprintf(command, sizeof(command), "echo -n '%s' | /usr/bin/wl-copy", content);
	wl_copy_ptr = popen(command, "w");
	if (wl_copy_ptr == NULL) {
		perror("Failed to open wl-copy");
		return ;
	}
	pclose(wl_copy_ptr);
}
#endif
static int report_event(unsigned int type, unsigned int code, unsigned int value) {
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
void *process_event_thread_func() {
	struct mouse_kbd_event mke;
	int ret;
	while (1) {
		struct sockaddr_in clnt_addr;
		socklen_t clnt_addr_len = sizeof(clnt_addr);
		client_sock_fd = accept(server_sock_fd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);


		while (1) {
			memset(&mke, 0, sizeof(mke));
			ret = recv(client_sock_fd, &mke, sizeof(mke), 0);
			if (ret <= 0) { 
				if (ret == 0) {
					printf("client disconnected\n");
					close(client_sock_fd);
					break;
				} else {
					printf("Failed to receive event\n");
				}
			} else {
				//print_event(event);
				if (mke.type == SET_MOUSE_POSITION) {
					global_x = mke.pos.x;
					global_y = mke.pos.y;
					set_mouse_position(global_x, global_y);
				} else if (mke.type == MOUSE_EVENT) {
					report_event(mke.ev.type, mke.ev.code, mke.ev.value);
#if ENABLE_COLIPBOARD
				} else if (mke.type == CLIPBOARD_EVENT) {
					// copy content to clipboard
					if (strcmp(clipboard_content, mke.data) == 0) 
						continue;
					//decrypt the message
					xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
					//
					printf("Received message '%s' from client\n", mke.data);
					strcpy(clipboard_content, mke.data);
					copy_to_clipboard((const char*)clipboard_content);
					printf("Now the content of clipboard is '%s'\n", clipboard_content);
				}
#else 
				}
#endif
			}
		}
	}
	return NULL;
}

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
	printf("start to listening--------------------\n");

}

int main() {
	int ret = 0;
	ret = creat_vir_input_device();
	if (ret < 0) {
		perror("Failed to create vir_input_device");
		exit(1);
	}

	init_socket();

	/* create a thread to process the event come from client */
	pthread_t process_event_thread;
	ret = pthread_create(&process_event_thread, NULL, process_event_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create process_event_thread successfully!");
	} else {
		DEBUG_INFO("Failed to create process_event_thread");
	}
	/* create a thread to watch mouse reached at the left boundary */
	pthread_t watch_mouse_thread;
	ret = pthread_create(&watch_mouse_thread, NULL, watch_mouse_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create watch_mouse_thread successfully!");
	} else {
		DEBUG_INFO("Failed to create watch_mouse_thread");
	}
	/* setting mouse position need to sleep 10ms so we create a thread to reduce the effect to main thread */
	pthread_t set_position_thread;
	ret = pthread_create(&set_position_thread, NULL, set_mouse_positon_func, NULL);
	if (ret == 0) {
		//pthread_setname_np(set_position_thread, "set_position_thread");
		DEBUG_INFO("create thread successfully");
	} else {
		DEBUG_INFO("Failed to create thread");
	}
#if ENABLE_COLIPBOARD
	/* a thread to listen the content of clipboard */
	pthread_t listen_clipboard_thread;
	ret = pthread_create(&listen_clipboard_thread, NULL, listen_clipboard_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create thread successfully");
	} else {
		DEBUG_INFO("Failed to create thread");
	}
#endif
	pthread_join(process_event_thread, NULL);
	pthread_join(watch_mouse_thread, NULL);
	pthread_join(set_position_thread, NULL);
#if ENABLE_COLIPBOARD
	pthread_join(listen_clipboard_thread, NULL);
#endif

	return 0;
}

 
