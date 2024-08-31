/*
 ***********************************************

 @Author:   zappen
 @Mail:     zp935675863@gmail.com
 @Date:     2024-07-13
 @FileName: input_device_shared.c
 ***********************************************
 */

#include <stdio.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>

#include <libinput.h>
#include <libudev.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/wait.h>

#include "detect_mouse_kbd_event.h"
#include "abs_mouse_device.h"
#include "debug_info.h"
#include "get_target_dev_info.h"
#include "crypto.h"

#define SERVER_PORT 0x1024

#define SET_MOUSE_POSITION 0x1234560
#define KBD_EVENT 0x1234561
#define MOUSE_EVENT 0x1234562
#define CLIPBOARD_EVENT 0x1234563

#define BUFFER_SIZE 1024

#define ENABLE_CLIPBOARD 1

#if ENABLE_CLIPBOARD
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
#if ENABLE_CLIPBOARD
		uint8_t data[BUFFER_SIZE];
#endif
	};
};

extern char right_dev_ip[IPLEN];

static int client_sock_fd;
int device_id = 0;
int epoll_fd = -1;
struct epoll_event events[20];

double global_x = 960, global_y = 540;
double pointer_speed = -2;
const char key[] = "123";//the decrypt key


#if ENABLE_CLIPBOARD
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
		double dx_unacce = libinput_event_pointer_get_dx_unaccelerated(pointer_event);
		double dy_unacce = libinput_event_pointer_get_dy_unaccelerated(pointer_event);
		//double dx_acce = libinput_event_pointer_get_dx(pointer_event);
		//double dy_acce = libinput_event_pointer_get_dy(pointer_event);

		global_x += dx_unacce * (1 + pointer_speed), global_y += dy_unacce * (1 + pointer_speed);

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
		}
		if (global_x >= 1920) {
			DEBUG_INFO("Mouse reached the right boundary");
			global_x = 1920;
			struct mouse_kbd_event mke;
			memset(&mke, 0, sizeof(mke));
			mke.type = SET_MOUSE_POSITION;
			mke.pos.x = 0;
			mke.pos.y = global_y;
			send(client_sock_fd, &mke, sizeof(mke), 0);
			device_id = 1;
			// block until receive the mouse event from dev2
			while (1) {
				memset(&mke, 0, sizeof(mke));
				recv(client_sock_fd, &mke, sizeof(mke), 0);
				if (mke.type == SET_MOUSE_POSITION) {
					global_y = mke.pos.y;
					global_x = mke.pos.x;
					set_mouse_position(global_x, global_y);
					device_id = 0;
					break;
#if ENABLE_CLIPBOARD
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

#if ENABLE_CLIPBOARD
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
		//encrypt the message
		printf("The original message:%s\n",mke.data);
		xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
		printf("The jiami message:%s\n",mke.data);
		//
		strcpy(mke.data, clipboard_content);
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

static struct uinput_user_dev vir_input_dev;
static int vir_input_fd;
static int mouse_fd;
static int kbd_fd;
#define MOUSE_DEV_PATH "/dev/input/event5"
#define KBD_DEV_PATH "/dev/input/event6"
static char *kbd_dev_path;
static char *mouse_dev_path;


static int creat_vir_input_device() {
	int ret = 0;
	vir_input_fd = open("/dev/uinput", O_RDWR | O_NDELAY);
	if (vir_input_fd < 0) {
		printf("error:%s:%d\n", __func__, __LINE__);
		return -1;//error process.
	}

	memset(&vir_input_dev, 0, sizeof(struct uinput_user_dev));
	snprintf(vir_input_dev.name, UINPUT_MAX_NAME_SIZE, "my-vir-input");
	vir_input_dev.id.version = 0x1;
	vir_input_dev.id.product = 0x2;
	vir_input_dev.id.bustype = BUS_VIRTUAL;

	ioctl(vir_input_fd, UI_SET_EVBIT, EV_SYN);
	ioctl(vir_input_fd, UI_SET_EVBIT, EV_KEY);

	for (int i = 0; i < 256; i++) {
		ioctl(vir_input_fd, UI_SET_KEYBIT, i);
	}

	ioctl(vir_input_fd, UI_SET_KEYBIT, BTN_LEFT); 
	ioctl(vir_input_fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(vir_input_fd, UI_SET_KEYBIT, BTN_MIDDLE);

	ioctl(vir_input_fd, UI_SET_EVBIT, EV_REL); 
	ioctl(vir_input_fd, UI_SET_RELBIT, REL_X);
	ioctl(vir_input_fd, UI_SET_RELBIT, REL_Y);
	ioctl(vir_input_fd, UI_SET_RELBIT, REL_WHEEL);

	ret = write(vir_input_fd, &vir_input_dev, sizeof(struct uinput_user_dev));
	if (ret < 0) {
		printf("error:%s:%d\n", __func__, __LINE__);
		return ret;//error process.
	}

	ret = ioctl(vir_input_fd, UI_DEV_CREATE);
	if (ret < 0) {
		printf("error:%s:%d\n", __func__, __LINE__);
		close(vir_input_fd);
		return ret;//error process.
	}
	return 0;
}

static int report_event(unsigned int type, unsigned int code, unsigned int value) {
	struct input_event ev;
	int ret;
	gettimeofday(&ev.time, NULL);
	ev.type = type;
	ev.code = code;
	ev.value = value;

	ret = write(vir_input_fd, &ev, sizeof(struct input_event));
	if (ret < 0) {
		printf("error:%s:%d\n", __func__, __LINE__);
		return ret;//error process.
	}
	return 0;
}

void *send_event_thread_func() {
	int ret = 0;
	/* 1.创建虚拟输入设�?? */
	ret = creat_vir_input_device();
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return NULL;//error process.
	}

	while(epoll_fd < 0); // block until epoll_fd has been created

	// 读取到的input设�?�数�?
	struct input_event event;
	while (1) {
		int event_num = epoll_wait(epoll_fd, events, 20, -1);
		for (int i = 0; i < event_num; i++) {
			ret = read(events[i].data.fd, &event, sizeof(event));
			if (ret == sizeof(event)) {
				if (device_id == 0) {
					report_event(event.type, event.code, event.value);
				} else if (device_id == 1) {
					struct mouse_kbd_event mke;
					memset(&mke, 0, sizeof(mke));
					mke.type = MOUSE_EVENT;
					mke.ev = event;
					send(client_sock_fd, &mke, sizeof(mke), 0);
				}
			}

		}
	}
	return NULL;
}
int add_to_epoll(int fd) {
	int ret;
	struct epoll_event e_ev;
	memset(&e_ev, 0, sizeof(e_ev));
	e_ev.events = EPOLLIN; // we are interested in coming
	e_ev.data.fd = fd;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e_ev);
	return ret;
}
void get_dev_file_path() {
	int cnt_mouse;
	int cnt_kbd;
	char **mouse_devices = detect_all_mouse(&cnt_mouse);
	char **kbd_devices = detect_all_kbd(&cnt_kbd);
	if (cnt_mouse) {
		mouse_dev_path = mouse_devices[0];
	} else { // 在虚拟机上测试用，防止没有usb鼠标
		mouse_dev_path = "/dev/input/event2";
	}
	DEBUG_INFO("mouse_dev_path:%s", mouse_dev_path);

	if (cnt_kbd) {
		kbd_dev_path = kbd_devices[0];
	} else {
		kbd_dev_path = "/dev/input/event1";
	}
	DEBUG_INFO("kbd_dev_path:%s", kbd_dev_path);
}
void *init_epoll_thread_func() {
	int ret;
	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		perror("Failedto create epoll_fd");
		return NULL;
	}
	mouse_fd = open(mouse_dev_path, O_RDONLY);
	if (mouse_fd <= 0) {
		printf("open %s device error!\n", mouse_dev_path);
		return NULL;
	}
	kbd_fd = open(kbd_dev_path, O_RDONLY);
	if (kbd_fd < 0) {
		perror("open %s device error!\n");
		return NULL;
	}
	// add mouse_fd and kbd_fd into epoll_fd
	ret = add_to_epoll(mouse_fd);
	if (ret < 0) {
		perror("failed to add mouse_fd");
	}
	ret = add_to_epoll(kbd_fd);
	if (ret < 0) {
		perror("failed to add kbd_fd");
	}
	return NULL;
}

void *build_socket_connect_thread_func() {
	int ret;
	struct sockaddr_in server_addr;
	/* 创建客户�?套接�? */
	client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sock_fd < 0) {
		perror("Failed to create socket");
		exit(EXIT_FAILURE);
	}

	/* 设置服务器地址信息 */
	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(right_dev_ip);
	server_addr.sin_port = htons(SERVER_PORT);

	/* 建立连接 */
	ret = connect(client_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perror("failed to connect to the server");
		exit(EXIT_FAILURE);
	}
	DEBUG_INFO("connect to another successfully\n");
	return NULL;
}

void get_mouse_speed() {
	/* file_path = "/home/tclab/.config/kcminputrc" */
	char file_path[32] = "/home/";
	int file_path_len = strlen(file_path);

	char *user_name = getlogin(); // user_name = tclab
	int user_name_len = strlen(user_name);

	strcpy(file_path + file_path_len, user_name); // file_path = /home/tclab
	file_path_len += user_name_len; // update file_path_len

	char *file_postfix = "/.config/kcminputrc";
	int file_postfix_len = strlen(file_postfix);

	strcpy(file_path + file_path_len, file_postfix);
	file_path_len += file_postfix_len; // update file_path_len
	file_path[file_path_len] = '\0';

	/* now file_path_len = /home/tclab/.config/kcminputrc */
	DEBUG_INFO("file_path:%s", file_path);

	FILE *file = fopen(file_path, "r");
	if (!file) {
		perror("Failed to open file");
		return ;
	}

	char line[100];
	int device_found = 0;
	while (fgets(line, sizeof(line), file)) {
		// Remove newline character
		line[strcspn(line, "\n")] = 0;

		// Check for the device section
		if (strstr(line, " Mouse")) { // various mouse have different name, but all of them include " Mouse"
			device_found = 1;
			continue;
		}

		// If device section is found, look for PointerAcceleration
		if (device_found) {
			if (strstr(line, "PointerAcceleration")) {
				char *token = strtok(line, "=");
				token = strtok(NULL, "=");
				if (token) {
					pointer_speed = atof(token);
				}
				break;
			}

			// If we encounter another section, stop searching
			if (line[0] == '[') {
				break;
			}
		}
	}
	fclose(file);
}

void *set_mouse_positon_func() {
	/* set mouse's absolute position in screen*/
	create_abs_mouse();
	usleep(100000);
	global_x = 960, global_y = 540;
	set_mouse_position(global_x, global_y);
	//ioctl(abs_mouse_fd, UI_DEV_DESTROY);
	//close(abs_mouse_fd);
	return NULL;
}

void Get_CtrlC_handler(int sig) {
	signal(sig, SIG_IGN);

	printf("捕捉到Ctrl-C\n");

	char* mouse_cmd = calloc(10086, sizeof(char));
	char* kbd_cmd = calloc(10086, sizeof(char));
	sprintf(mouse_cmd, "sudo udevadm trigger --action=add %s", mouse_dev_path);
	sprintf(kbd_cmd, "sudo udevadm trigger --action=add %s", kbd_dev_path);
	system(mouse_cmd);
	system(kbd_cmd);
	free(mouse_cmd);
	free(kbd_cmd);

	exit(0);
}

int main() {
	get_dev_file_path();
	signal(SIGINT, Get_CtrlC_handler);
	int ret;

	/* setting mouse position need to sleep 10ms so we create a thread to reduce the effect to main thread */
	pthread_t set_position_thread;
	ret = pthread_create(&set_position_thread, NULL, set_mouse_positon_func, NULL);
	if (ret == 0) {
		//pthread_setname_np(set_position_thread, "set_position_thread");
		DEBUG_INFO("create thread successfully");
	} else {
		DEBUG_INFO("Failed to create thread");
	}

	/* get config info, such as server ip */
	get_info();
	DEBUG_INFO("right device ip : %s", right_dev_ip);

	/* get mouse's speed */
	get_mouse_speed();

	/* create a thread to build connection with another device */
	pthread_t build_socket_connect_thread;
	ret = pthread_create(&build_socket_connect_thread, NULL, build_socket_connect_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create build_socket_connect_thread successfully");
	} else {
		DEBUG_INFO("Failed to create build_socket_connect_thread");
	}
	/* create a thread to init epoll */
	pthread_t init_epoll_thread;
	ret = pthread_create(&init_epoll_thread, NULL, init_epoll_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create init_epoll_thread successfully");
	} else {
		DEBUG_INFO("Failed to create init_epoll_thread");
	}
	/* create a thread to send keyboard and mouse event */
	pthread_t send_event_thread;
	ret = pthread_create(&send_event_thread, NULL, send_event_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create send_event_thread successfully");
	} else {
		DEBUG_INFO("Failed to create send_event_thread");
	}
	pthread_t watch_mouse_thread;
	ret = pthread_create(&watch_mouse_thread, NULL, watch_mouse_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create send_event_thread successfully");
	} else {
		DEBUG_INFO("Failed to create send_event_thread");
	}
#if ENABLE_CLIPBOARD
	/* a thread to listen the content of clipboard */
	pthread_t listen_clipboard_thread;
	ret = pthread_create(&listen_clipboard_thread, NULL, listen_clipboard_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create thread successfully");
	} else {
		DEBUG_INFO("Failed to create thread");
	}
#endif 

	/* deactivate mouse and keyboard */
	char* mouse_cmd = calloc(10086, sizeof(char));
	char* kbd_cmd = calloc(10086, sizeof(char));
	sprintf(mouse_cmd, "sudo udevadm trigger --action=remove %s", mouse_dev_path);
	sprintf(kbd_cmd, "sudo udevadm trigger --action=remove %s", kbd_dev_path);
	system(mouse_cmd);
	system(kbd_cmd);

	free(mouse_cmd);
	free(kbd_cmd);

	pthread_join(set_position_thread, NULL);
	pthread_join(build_socket_connect_thread, NULL);
	pthread_join(init_epoll_thread, NULL);
	pthread_join(send_event_thread, NULL);
	pthread_join(watch_mouse_thread, NULL);
#if ENABLE_CLIPBOARD
	pthread_join(listen_clipboard_thread, NULL);
#endif
	return 0;
}

