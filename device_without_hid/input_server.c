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
#include "get_resolution.h"

#define SERVER_PORT 0x1024


#define SET_MOUSE_POSITION 0x1234560
#define KBD_EVENT 0x1234561
#define MOUSE_EVENT 0x1234562

#define DEVICE_SEAT_UP 0x1234566
#define DEVICE_SEAT_DOWN 0x1234567
#define DEVICE_SEAT_LEFT 0x1234568
#define DEVICE_SEAT_RIGHT 0x1234569

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
	int sit;
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

int g_device_seat;
int g_screen_width;
int g_screen_height;


double global_x, global_y;
double pointer_speed = -2;
int mouse_accel = 1;
int enable_encrypt = 0;

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

static void send_clipboard(int sock_fd, struct mouse_kbd_event mke) {
	//send clipboard content
	printf("%s\n", __FUNCTION__);
	copy_from_clipboard();
	//if (strcmp(clipboard_content, send_buf) != 0) {// have changed
	if (1) {// have changed
		memset(&mke, 0, sizeof(mke));
		mke.type = CLIPBOARD_EVENT;
		strcpy(mke.data, clipboard_content);
		if(enable_encrypt) {
			xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
		}
		int ret = send(sock_fd, &mke, sizeof(mke), 0);
		if (ret < 0) {
			printf("Failed to send mke.data to client\n");
		}
	}
}

static void handle_event(struct libinput_event *event) {
    if (libinput_event_get_type(event) == LIBINPUT_EVENT_POINTER_MOTION) {
        struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event(event);
		double dx, dy;
		if(mouse_accel == 0){
			dx = libinput_event_pointer_get_dx_unaccelerated(pointer_event);
			dy = libinput_event_pointer_get_dy_unaccelerated(pointer_event);
		}
		else if(mouse_accel == 1){
			dx = libinput_event_pointer_get_dx(pointer_event);
			dy = libinput_event_pointer_get_dy(pointer_event);
		}
		global_x += dx * (1 + pointer_speed), global_y += dy * (1 + pointer_speed);

		/* we assume we have two device, dev1 and dev2, 
		 * their screen is 1920x1080 and their position follow: 
		 *         ____________________     ____________________
		 *  	  |					   |   |					|
		 *		  |                    |   |                    |
		 *        |        dev1        |   |        dev2        |
		 *        |                    |   |                    |
		 *        |____________________|   |____________________|
		 */
		if (global_x < 0) {
			DEBUG_INFO("Mouse reached the left boundary");
			global_x = 0;
			if (g_device_seat == DEVICE_SEAT_RIGHT) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.pos.x = g_screen_width;
				mke.pos.y = global_y / g_screen_height;
				send(client_sock_fd, &mke, sizeof(mke), 0);
#if ENABLE_COLIPBOARD

				send_clipboard(client_sock_fd, mke);
#endif	
			}
		}
		if (global_x >= g_screen_width) {
			DEBUG_INFO("Mouse reached the right boundary");
			global_x = g_screen_width;
			if (g_device_seat == DEVICE_SEAT_LEFT) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.pos.x = 0;
				mke.pos.y = global_y / g_screen_height;
				send(client_sock_fd, &mke, sizeof(mke), 0);
#if ENABLE_COLIPBOARD
				send_clipboard(client_sock_fd, mke);
#endif	
			}
		}
		if (global_y <= 0) {
			DEBUG_INFO("Mouse reached the top boundary");
			global_y = 0;
			if (g_device_seat == DEVICE_SEAT_DOWN) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.pos.x = global_x / g_screen_width;
				mke.pos.y = g_screen_height;
				send(client_sock_fd, &mke, sizeof(mke), 0);
#if ENABLE_COLIPBOARD
				send_clipboard(client_sock_fd, mke);
#endif	
			}
		}
		if (global_y >= g_screen_height) {
			DEBUG_INFO("Mouse reached the bottom boundary");
			global_y = g_screen_height;
			if (g_device_seat == DEVICE_SEAT_UP) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.pos.x = global_x / g_screen_width;
				mke.pos.y = 0;
				send(client_sock_fd, &mke, sizeof(mke), 0);
#if ENABLE_COLIPBOARD
				send_clipboard(client_sock_fd, mke);
#endif	
			}
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
	char buffer[BUFFER_SIZE];
	clipboard_content[0] = '\0';  // 初始化 clipboard_content 为空字符串
	// 循环读取直到文件结束
    while (fgets(buffer, BUFFER_SIZE, wl_paste_ptr) != NULL) {
        // 拼接读取到的内容到 clipboard_content
        strncat(clipboard_content, buffer, BUFFER_SIZE - strlen(clipboard_content) - 1);
    }
	char *last_newline = strrchr(clipboard_content, '\n');
	if (last_newline != NULL) {
        // 将最后一个换行符替换为字符串结束符
        *last_newline = '\0';
    }
	// clipboard_content[strcspn(clipboard_content, "\n")] = '\0';
	pclose(wl_paste_ptr);
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
	usleep(1000000);
	//global_x = g_screen_width / 2, global_y = g_screen_height / 2;
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
				if (mke.type == SET_MOUSE_POSITION) {
					g_device_seat = mke.sit;
					if (g_device_seat == DEVICE_SEAT_UP) {
						global_x = mke.pos.x * g_screen_width;
						global_y = g_screen_height;
					} else if (g_device_seat == DEVICE_SEAT_DOWN) {
						global_x = mke.pos.x * g_screen_width;
						global_y = 0;
					} else if (g_device_seat == DEVICE_SEAT_LEFT) {
						global_x = g_screen_width;
						global_y = mke.pos.y * g_screen_height;
					} else if (g_device_seat == DEVICE_SEAT_RIGHT) {
						global_x = 0;
						global_y = mke.pos.y * g_screen_height;
					}
					set_mouse_position(global_x, global_y);
				} else if (mke.type == MOUSE_EVENT) {
					report_event(mke.ev.type, mke.ev.code, mke.ev.value);
#if ENABLE_COLIPBOARD
				} else if (mke.type == CLIPBOARD_EVENT) {
					// copy content to clipboard
					if (strcmp(clipboard_content, mke.data) == 0) 
						continue;
					if(enable_encrypt) {
						//decrypt the message
						xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
					}
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
	if (server_sock_fd < 0) {
		perror("Failed to create server_sock_fd");
	}

	int on = 1;
	ret = setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (ret < 0) {
		perror("Failed to setsockopt");
	}
	ret = setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	if (ret < 0) {
		perror("Failed to setsockopt SO_REUSEPORT");
	}

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

void get_mouse_speed() {
	FILE *fp;
    char buffer[1024];

    // 根据不同的操作系统类型，执行不同的命令
	#if defined(__linux__)
		//检查桌面服务器（X11？Waylnad？）
		const char* wayland_display = getenv("WAYLAND_DISPLAY");
		const char* x11_display = getenv("DISPLAY");
		if(wayland_display != NULL){
			printf("This is a Wayland!\n");
			// 检查桌面服务器（UKUI、GNOME等）
			const char* desktop_env = getenv("XDG_CURRENT_DESKTOP");
			if (desktop_env == NULL){
				perror("Failed to get Desktop Environment!\n");
				exit(1);
			}
			// 根据桌面服务器进行不同操作
			if (strstr(desktop_env, "GNOME") != NULL) {
				printf("GNOME Desktop Environment\n");
				// 获取当前用户名
				const char *username = getlogin();
				if (username == NULL) {
					fprintf(stderr, "Error: Unable to get the current username.\n");
					return;
				}
				printf("The username:%s\n",username);
				// 构建要执行的命令
				char cmd[256];
				snprintf(cmd, sizeof(cmd), "su %s -c \"%s\"", username, "gsettings set org.gnome.desktop.peripherals.mouse speed 0");
				// 执行命令
				int ret = system(cmd);
				if (ret == -1) {
					perror("system");
				}
				fp = popen("gsettings get org.gnome.desktop.peripherals.mouse speed", "r");
				// 读取命令输出
				if (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					// 将字符串转换为浮点数
					pointer_speed = atof(buffer);
					if(pointer_speed >= 1){
						pointer_speed = 1;//openkylin上面特殊处理
					}
					printf("The speed:%f\n", pointer_speed);
				} else {
					fprintf(stderr, "No output from command.\n");
				}
				pclose(fp);
			} else if (strstr(desktop_env, "UKUI") != NULL) {
				printf("UKUI Desktop Environment\n");
				//判断鼠标加速是否开启
				fp = popen("gsettings get org.ukui.peripherals-mouse mouse-accel", "r");
					// 读取命令输出
				if (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					if(strstr(buffer, "true") != NULL){
						printf("The mouse acceleration is opened!\n");
						mouse_accel = 1;
					}
					else{
						printf("The mouse acceleration is closed!\n");
						mouse_accel = 0;
					}
				} else {
					fprintf(stderr, "No output from command.\n");
				}
				if(mouse_accel){//开启鼠标加速的话把速度设置到0
					// 获取当前用户名
					const char *username = getlogin();
					if (username == NULL) {
						fprintf(stderr, "Error: Unable to get the current username.\n");
						return;
					}
					printf("The username:%s\n",username);
					// 构建要执行的命令
					char cmd[256];
					snprintf(cmd, sizeof(cmd), "su %s -c \"%s\"", username, "gsettings set org.ukui.peripherals-mouse motion-acceleration 4.5");
					// 执行命令
					int ret = system(cmd);
					if (ret == -1) {
						perror("system");
					}
				}
				//获取鼠标速度
				fp = popen("gsettings get org.ukui.peripherals-mouse motion-acceleration", "r");
				// 读取命令输出
				if (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
					// 将字符串转换为浮点数
					pointer_speed = atof(buffer);
					pointer_speed = 0.285714*pointer_speed -1.285714;//线性回归出来的
					if(pointer_speed >= 1){
						pointer_speed = 1;//openkylin上面特殊处理
					}
					printf("The speed:%f\n", pointer_speed);
				} else {
					fprintf(stderr, "No output from command.\n");
				}
				pclose(fp);
			} 
			else {
				fp = popen("echo 'Unknown desktop-server'", "r");
			}
		}
		else if(x11_display != NULL){
			printf("This is a X11!\n");
			exit(0);//暂时未作处理
		}
		
		
	#elif defined(_WIN32) || defined(_WIN64)
		// Windows 系统
		fp = popen("ver", "r");
	#elif defined(__APPLE__) || defined(__MACH__)
		// macOS 系统
		fp = popen("sw_vers", "r");
	#else
		// 其他系统
		fp = popen("echo 'Unsupported OS'", "r");
	#endif

}

void Get_CtrlC_handler(int sig) {
	signal(sig, SIG_IGN);
	close(server_sock_fd);
	printf("捕捉到Ctrl-C\n");


	exit(0);
}

int main() {
	int ret = 0;
	signal(SIGINT, Get_CtrlC_handler);
	ret = creat_vir_input_device();
	get_mouse_speed();
	if (ret < 0) {
		perror("Failed to create vir_input_device");
		exit(1);
	}
	
	init_socket();

	// TODO: 动态改变分辨率
	g_screen_width = 1920;
	g_screen_height = 1080;
	get_resolution(&g_screen_width, &g_screen_height);
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
	pthread_join(process_event_thread, NULL);
	pthread_join(watch_mouse_thread, NULL);
	pthread_join(set_position_thread, NULL);

	return 0;
}

 
