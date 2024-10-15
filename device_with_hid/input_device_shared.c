/* Copyright (C) 2024 赵鹏 (Peng Zhao) <224712239@csu.edu.cn>, 王振锋 (Zhenfeng Wang) <234711103@csu.edu.cn>, 杨纪琛 (Jichen Yang) <234712186@csu.edu.cn>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
#include <sys/inotify.h>
#include "get_resolution.h"

#define INOTIFY_EVENT_NUM  12

#include "detect_mouse_kbd_event.h"
#include "abs_mouse_device.h"
#include "debug_info.h"
#include "get_target_dev_info.h"
#include "base.h"
#include "crypto.h"

#define SERVER_PORT 0x1024

#define SET_MOUSE_POSITION 0x1234560
#define KBD_EVENT 0x1234561
#define MOUSE_EVENT 0x1234562
#define CLIPBOARD_EVENT 0x1234563

#define DEVICE_SEAT_UP 0x1234566
#define DEVICE_SEAT_DOWN 0x1234567
#define DEVICE_SEAT_LEFT 0x1234568
#define DEVICE_SEAT_RIGHT 0x1234569

#define DEVICE_ID_UP 0x1234571
#define DEVICE_ID_DOWN 0x1234572
#define DEVICE_ID_LEFT 0x1234573
#define DEVICE_ID_RIGHT 0x1234574
#define DEVICE_ID_LOCAL 0x1234575

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
	int sit;  // 告诉从设备，从设备位于主设备的方位信息
	union {
		struct input_event ev;
		mouse_positon pos;
#if ENABLE_CLIPBOARD
		uint8_t data[BUFFER_SIZE];
#endif
	};
};


#define DEVICE_COUNT 6
struct servant_device servant_devices[DEVICE_COUNT];
int left_device_index = -1;
int right_device_index = -1;
int up_device_index = -1;
int down_device_index = -1;

int g_screen_width;
int g_screen_height;
int g_device_id = DEVICE_ID_LOCAL;

int epoll_fd = -1;
struct epoll_event events[20];

double global_x, global_y;
int mouse_accel = 1;
int enable_encrypt = 0;
double pointer_speed = -2;
const char key[] = "123";//the decrypt key

static int get_free_index() {
	for (int i = 0; i < DEVICE_COUNT; i++) {
		if (servant_devices[i].flag == 0) {
			return i;
		}
	}
	return -1;
}
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

#if ENABLE_CLIPBOARD
void copy_from_clipboard();
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
#endif

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

		/* we assume we have three PC, PC1, PC2 and PC3, 
		 * their screen's resolution is 1920x1080 and their layout follow: 
		 *         ____________________     ____________________    ____________________ 
		 *  	  |					   |   |					|  |					|
		 *		  |                    |   |                    |  |                    |
		 *        |        PC1         |   |        PC2         |  |        PC3         |
		 *        |                    |   |                    |  |                    |
		 *        |____________________|   |____________________|  |____________________|
		 *
		 *  in these devices, only PC2 owns hid input device.
		 */
		if (global_x <= 0) {
			DEBUG_INFO("Mouse reached the left boundary");
			global_x = 0;
			if (left_device_index >= 0) {

				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.sit = DEVICE_SEAT_LEFT;
				mke.pos.x = g_screen_width;
				mke.pos.y = global_y / g_screen_height;

				send(servant_devices[left_device_index].sock_fd, &mke, sizeof(mke), 0);
				g_device_id = DEVICE_ID_LEFT;
#if ENABLE_CLIPBOARD
				send_clipboard(servant_devices[left_device_index].sock_fd, mke);
#endif	
				// block until receive the mouse event from other PC
				while (1) {
					memset(&mke, 0, sizeof(mke));
					int ret = recv(servant_devices[left_device_index].sock_fd, &mke, sizeof(mke), 0);
					if (ret <= 0) {
						break;
					}
					if (mke.type == SET_MOUSE_POSITION) {
						global_x = 0;
						global_y = mke.pos.y * g_screen_height;
						set_mouse_position(global_x, global_y);
						g_device_id = DEVICE_ID_LOCAL;
#if ENABLE_CLIPBOARD
					} else if (mke.type == CLIPBOARD_EVENT) {
						// copy content to clipboard
						if (strcmp(clipboard_content, mke.data) == 0) 
							break;
						if(enable_encrypt) {
							//decrypt the message
							xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
						}
						
						printf("Received message '%s' from client\n", mke.data);
						strcpy(clipboard_content, mke.data);
						copy_to_clipboard((const char*)clipboard_content);
						printf("Now the content of clipboard is '%s'\n", clipboard_content);
						break;
					}
#else
					} // end else if
#endif 
				} // end while 

			} // end if (right_dev_enable) 
		}
		if (global_x > g_screen_width) {
			DEBUG_INFO("Mouse reached the right boundary");
			global_x = g_screen_width;
			if (right_device_index >= 0) {
			//if (cur_devices_layout.right) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.sit = DEVICE_SEAT_RIGHT;
				mke.pos.x = 0;
				mke.pos.y = global_y / g_screen_height;
				send(servant_devices[right_device_index].sock_fd, &mke, sizeof(mke), 0);
				g_device_id = DEVICE_ID_RIGHT;
#if ENABLE_CLIPBOARD
				send_clipboard(servant_devices[right_device_index].sock_fd, mke);
#endif
				// block until receive the mouse event from dev2
				while (1) {
					memset(&mke, 0, sizeof(mke));
					int ret = recv(servant_devices[right_device_index].sock_fd, &mke, sizeof(mke), 0);
					if (ret <= 0) {
						perror("Failed to recv");
						break;
					}
					if (mke.type == SET_MOUSE_POSITION) {
						global_x = g_screen_width;
						global_y = mke.pos.y * g_screen_height;
						set_mouse_position(global_x, global_y);
						g_device_id = DEVICE_ID_LOCAL;
						
						
#if ENABLE_CLIPBOARD
					} else if (mke.type == CLIPBOARD_EVENT) {
						// copy content to clipboard
						if (strcmp(clipboard_content, mke.data) == 0) 
							break;
						if(enable_encrypt) {
							//decrypt the message
							xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
						}
						printf("Received message '%s' from client\n", mke.data);
						strcpy(clipboard_content, mke.data);
						copy_to_clipboard((const char*)clipboard_content);
						printf("Now the content of clipboard is '%s'\n", clipboard_content);
						break;
					}
#else
					} // end else if
#endif 
				} // end while 

			} // end if (right_dev_enable) 
		} // end if (global_x >= 1920)
		if (global_y <= 0) {
			DEBUG_INFO("Mouse reached the top boundary");
			global_y = 0;
			if (up_device_index >= 0) {
			//if (cur_devices_layout.up) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.sit = DEVICE_SEAT_UP;
				mke.pos.x = global_x / g_screen_width;
				mke.pos.y = 0;
				send(servant_devices[up_device_index].sock_fd, &mke, sizeof(mke), 0);
				g_device_id = DEVICE_ID_UP;
#if ENABLE_CLIPBOARD
				send_clipboard(servant_devices[up_device_index].sock_fd, mke);
#endif
				while (1) {
					memset(&mke, 0, sizeof(mke));
					int ret = recv(servant_devices[up_device_index].sock_fd, &mke, sizeof(mke), 0);
					if (ret <= 0) {
						break;
					}
					if (mke.type == SET_MOUSE_POSITION) {
						global_x = mke.pos.x * g_screen_width;
						global_y = 0;
						set_mouse_position(global_x, global_y);
						g_device_id = DEVICE_ID_LOCAL;
#if ENABLE_CLIPBOARD
					} else if (mke.type == CLIPBOARD_EVENT) {
						// copy content to clipboard
						if (strcmp(clipboard_content, mke.data) == 0) 
							break;
						if(enable_encrypt) {
							//decrypt the message
							xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
						}
						
						printf("Received message '%s' from client\n", mke.data);
						strcpy(clipboard_content, mke.data);
						copy_to_clipboard((const char*)clipboard_content);
						printf("Now the content of clipboard is '%s'\n", clipboard_content);
						break;
					}
#else
					} // end else if
#endif 
				} // end while 

			} // end if (right_dev_enable) 

		}
		if (global_y >= g_screen_height) {
			DEBUG_INFO("Mouse reached the bottom boundary");
			global_y = g_screen_height;
			printf("down_device_index:%d\n", down_device_index);
			if (down_device_index >= 0) {
			//if (cur_devices_layout.down) {
				struct mouse_kbd_event mke;
				memset(&mke, 0, sizeof(mke));
				mke.type = SET_MOUSE_POSITION;
				mke.sit = DEVICE_SEAT_DOWN;
				mke.pos.x = global_x / g_screen_width;
				mke.pos.y = 0;
				send(servant_devices[down_device_index].sock_fd, &mke, sizeof(mke), 0);
				g_device_id = DEVICE_ID_DOWN;
#if ENABLE_CLIPBOARD
				send_clipboard(servant_devices[down_device_index].sock_fd, mke);
#endif
				while (1) {
					memset(&mke, 0, sizeof(mke));
					int ret = recv(servant_devices[down_device_index].sock_fd, &mke, sizeof(mke), 0);
					if (ret <= 0) {
						break;
					}
					if (mke.type == SET_MOUSE_POSITION) {
						global_x = mke.pos.x * g_screen_width;
						global_y = g_screen_height;
						set_mouse_position(global_x, global_y);
						g_device_id = DEVICE_ID_LOCAL;
#if ENABLE_CLIPBOARD
					} else if (mke.type == CLIPBOARD_EVENT) {
						// copy content to clipboard
						if (strcmp(clipboard_content, mke.data) == 0) 
							break;
						if(enable_encrypt) {
							//decrypt the message
							xor_encrypt_decrypt(mke.data, strlen(mke.data), key, strlen(key));
						}
						
						printf("Received message '%s' from client\n", mke.data);
						strcpy(clipboard_content, mke.data);
						copy_to_clipboard((const char*)clipboard_content);
						printf("Now the content of clipboard is '%s'\n", clipboard_content);
						break;
					}
#else
					} // end else if
#endif 
				} // end while 

			} // end if (right_dev_enable) 
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
	char buffer[BUFFER_SIZE];
	clipboard_content[0] = '\0';  // 初始化 clipboard_content 为空字符串
	//循环读取直到文件结束
    while (fgets(buffer, BUFFER_SIZE, wl_paste_ptr) != NULL) {
        // 拼接读取到的内容到 clipboard_content
		// printf("get a line!\n");
        strncat(clipboard_content, buffer, BUFFER_SIZE - strlen(clipboard_content) - 1);
    }
	// fgets(clipboard_content, BUFFER_SIZE, wl_paste_ptr);
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

static struct uinput_user_dev vir_input_dev;
static int vir_input_fd;
static int mouse_fd;
static int kbd_fd;
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
	ret = creat_vir_input_device();
	if(ret < 0){
		printf("%s:%d\n", __func__, __LINE__);
		return NULL;//error process.
	}

	while(epoll_fd < 0); // block until epoll_fd has been created

	struct input_event event;
	int failed_times = 0;
	while (1) {
		int event_num = epoll_wait(epoll_fd, events, 20, -1);
		for (int i = 0; i < event_num; i++) {
			ret = read(events[i].data.fd, &event, sizeof(event));
			if (ret == sizeof(event)) {
				if (g_device_id == DEVICE_ID_LOCAL) {
					report_event(event.type, event.code, event.value);
				} else {
					struct mouse_kbd_event mke;
					memset(&mke, 0, sizeof(mke));
					mke.type = MOUSE_EVENT;
					mke.ev = event;
					int cur_sockfd;
					if (g_device_id == DEVICE_ID_UP) {
						ret = send(servant_devices[up_device_index].sock_fd, &mke, sizeof(mke), 0);
						if (ret <= 0) {
							failed_times++;
							if (failed_times == 3) {
								failed_times = 0;
								g_device_id = DEVICE_ID_LOCAL;
							}
						}

					} else if (g_device_id == DEVICE_ID_DOWN) {
						ret = send(servant_devices[down_device_index].sock_fd, &mke, sizeof(mke), 0);
						if (ret <= 0) {
							perror("Failed to send to down");
							failed_times++;
							if (failed_times == 3) {
								failed_times = 0;
								g_device_id = DEVICE_ID_LOCAL;
							}
						}
					} else if (g_device_id == DEVICE_ID_LEFT) {
						ret = send(servant_devices[left_device_index].sock_fd, &mke, sizeof(mke), 0);
						if (ret <= 0) {
							failed_times++;
							if (failed_times == 3) {
								failed_times = 0;
								g_device_id = DEVICE_ID_LOCAL;
							}
						}
					} else {
						ret = send(servant_devices[right_device_index].sock_fd, &mke, sizeof(mke), 0);
						if (ret <= 0) {
							//printf("Failed to send\n");
							failed_times++;
							if (failed_times == 3) {
								failed_times = 0;
								g_device_id = DEVICE_ID_LOCAL;
							}
						}

					}
				}
			}

		}
	}
	return NULL;
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

int add_to_epoll(int fd) {
	int ret;
	struct epoll_event e_ev;
	memset(&e_ev, 0, sizeof(e_ev));
	e_ev.events = EPOLLIN; // we are interested in coming
	e_ev.data.fd = fd;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e_ev);
	return ret;
}

void add_kbd_mouse_to_epollfd() {
	int ret;
	mouse_fd = open(mouse_dev_path, O_RDONLY);
	if (mouse_fd <= 0) {
		printf("open %s device error!\n", mouse_dev_path);
	}
	kbd_fd = open(kbd_dev_path, O_RDONLY);
	if (kbd_fd < 0) {
		perror("open %s device error!\n");
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
				snprintf(cmd, sizeof(cmd), "gsettings set org.gnome.desktop.peripherals.mouse speed 0");
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
					snprintf(cmd, sizeof(cmd), "gsettings set org.ukui.peripherals-mouse motion-acceleration 4.5");
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

const char *event_str[INOTIFY_EVENT_NUM] = {
	"IN_ACCESS",
	"IN_MODIFY",
	"IN_ATTRIB",
	"IN_CLOSE_WRITE",
	"IN_CLOSE_NOWRITE",
	"IN_OPEN",
	"IN_MOVED_FROM",
	"IN_MOVED_TO",
	"IN_CREATE",
	"IN_DELETE",
	"IN_DELETE_SELF",
	"IN_MOVE_SELF"
};

void printf_devices_layout(struct devices_layout layout) {
	if (layout.up) {
		printf("up device ip is %s, sockfd is %d\n", layout.up->ip, layout.up->sock_fd);
	}
	if (layout.down) {
		printf("down device ip is %s, sockfd is %d\n", layout.down->ip, layout.down->sock_fd);
	}
	if (layout.left) {
		printf("left device ip is %s, sockfd is %d\n", layout.left->ip, layout.left->sock_fd);
	}
	if (layout.right) {
		printf("right device ip is %s, sockfd is %d\n", layout.right->ip, layout.right->sock_fd);
	}
}
void connect_to_server(int *fd, char *ip) {
	int ret;
	struct sockaddr_in server_addr;
	*fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*fd < 0) {
		perror("Failed to create socket");
		exit(EXIT_FAILURE);
	}

	/* 设置服务器地址信息 */
	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(SERVER_PORT);

	ret = connect(*fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		perror("failed to connect to the server");
		exit(EXIT_FAILURE);
	}
	DEBUG_INFO("connect to another successfully\n");
}

void update_layout(int servant_device_index, char *pos) {
	if (strcmp(pos, "up") == 0) {
		up_device_index = servant_device_index;
	} else if (strcmp(pos, "down") == 0) {
		down_device_index = servant_device_index;
	} else if (strcmp(pos, "left") == 0) {
		left_device_index = servant_device_index;
	} else {
		right_device_index = servant_device_index;
	}
}
int find_device_info_by_ip(char *ip, char *pos) {
	for (int i = 0; i < DEVICE_COUNT; i++) {
		if (servant_devices[i].flag && strcmp(servant_devices[i].ip, ip) == 0) {
			update_layout(i, pos);
			return 0;
		}
	}
	return -1;
}
void update_status() {
	// 将当前布局的上下左右指针置为空
	up_device_index = -1;
	down_device_index = -1;
	left_device_index = -1;
	right_device_index = -1;
	// 1.get info of config.json
	char **device_ip_list = NULL;
	char **device_pos_list = NULL;
	int device_num = 0;
	char **device_removed_list = NULL;
	int device_removed_num = 0;
	get_servant_devices_info(&device_ip_list, &device_pos_list, &device_num, &device_removed_list, &device_removed_num);

	for (int i = 0; i < device_num; i++) {
		char *ip = device_ip_list[i];
		char *pos = device_pos_list[i];
		int res = find_device_info_by_ip(ip, pos);
		if (res == -1) {
			// 没找到
			printf("no found\n");
			int idx = get_free_index();
			servant_devices[idx].flag = 1;
			strcpy(servant_devices[idx].ip, ip);
			connect_to_server(&servant_devices[idx].sock_fd, ip);
			update_layout(idx, pos);
		}
	}
	for (int i = 0; i < device_removed_num; i++) {
		char *ip = device_removed_list[i];
		// update array of servant_devices
		for (int i = 0; i < DEVICE_COUNT; i++) {
			if (servant_devices[i].flag && strcmp(servant_devices[i].ip, ip) == 0) {
				servant_devices[i].flag = 0;
				close(servant_devices[i].sock_fd);
				servant_devices[i].sock_fd = -1;
				break;
			}
		}
	}
	for (int i = 0; i < device_num; i++) {
		free(device_ip_list[i]);
		free(device_pos_list[i]);
	}
	for (int i = 0; i < device_removed_num; i++) {
		free(device_removed_list[i]);
	}
	free(device_ip_list);
	free(device_pos_list);
	free(device_removed_list);
}

int watch_config_handler() {
	int errTimes = 0;
	int fd = -1;
	int counter = 0;
	fd = inotify_init();
	if (fd < 0) {
		fprintf(stderr, "inotify_init failed\n");
		printf("Error no.%d: %s\n", errno, strerror(errno));
		goto INOTIFY_FAIL;
	}

	int wd = -1;
	struct inotify_event *event;
	int length;
	int nread;
	char buf[BUFSIZ];
	int i = 0;
	buf[sizeof(buf) - 1] = 0;
INOTIFY_AGAIN:
	wd = inotify_add_watch(fd, "config.json", IN_ALL_EVENTS);
	if (wd < 0) {
		fprintf(stderr, "inotify_add_watch  failed\n");

		printf("Error no.%d: %s\n", errno, strerror(errno));

		if(errTimes < 3) {
			goto INOTIFY_AGAIN;
		}
		else {
			goto INOTIFY_FAIL;
		}
	}
	length = read(fd, buf, sizeof(buf) - 1);
	nread = 0;
	// inotify 事件发生时
	while(length > 0) {
		//printf("\n");
		event = (struct inotify_event *)&buf[nread];
		// 遍历所有事件
		for(i = 0; i< INOTIFY_EVENT_NUM; i++)
		{
			// 判断事件是否发生
			if( (event->mask >> i) & 1 )
			{
				// 该监控项为目录或目录下的文件时
				if(event->len > 0)
				{
					fprintf(stdout, "%s --- %s\n", event->name, event_str[i]);
				}
				// 该监控项为文件时
				else if(event->len == 0) {
					if (strcmp(event_str[i], "IN_MODIFY") == 0) {
						printf("watch config.json is modified\n");
						counter++;
						if (counter == 2) {
							counter = 0;
							printf("config.json is update\n");
							update_status();
							//printf_devices_layout(cur_devices_layout);
						}
					}
				}
			}
		}

		nread = nread + sizeof(struct inotify_event) + event->len;

		length = length - sizeof(struct inotify_event) - event->len;
	}

	goto INOTIFY_AGAIN;

	close(fd);

	return 0;

INOTIFY_FAIL:
	return -1;
}

void *watch_config_thread_func() {
	watch_config_handler();
	return NULL;
}

void *set_mouse_positon_func() {
	/* set mouse's absolute position in screen*/
	create_abs_mouse();
	usleep(500000);
	//global_x = 960, global_y = 540;
	global_x = g_screen_width / 2, global_y = g_screen_height / 2 ;
	set_mouse_position(global_x, global_y);
	//ioctl(abs_mouse_fd, UI_DEV_DESTROY);
	//close(abs_mouse_fd);
	return NULL;
}

void Get_CtrlC_handler(int sig) {
	signal(sig, SIG_IGN);

	printf("捕捉到Ctrl-C\n");

	char* mouse_cmd = calloc(100, sizeof(char));
	char* kbd_cmd = calloc(100, sizeof(char));
	sprintf(mouse_cmd, "udevadm trigger --action=add %s", mouse_dev_path);
	sprintf(kbd_cmd, "udevadm trigger --action=add %s", kbd_dev_path);
	system(mouse_cmd);
	system(kbd_cmd);
	free(mouse_cmd);
	free(kbd_cmd);

	exit(0);
}

int main() {
	int ret;
	signal(SIGINT, Get_CtrlC_handler);
	//pthread_mutex_init(&g_layout_lock, NULL);

	// TODO: 鼠标键盘热插拔
	get_dev_file_path();

	// TODO: 分辨率动态检测
	g_screen_width = 0;
	g_screen_height = 0;
	get_resolution(&g_screen_width, &g_screen_height);
	printf("cur screen is %dx%d\n", g_screen_width, g_screen_height);
	//global_x = g_screen_width >> 1;
	//global_y = g_screen_height >> 1;

	/* setting mouse position need to sleep 10ms so we create a thread to reduce the effect to main thread */
	pthread_t set_position_thread;
	ret = pthread_create(&set_position_thread, NULL, set_mouse_positon_func, NULL);
	if (ret == 0) {
		//pthread_setname_np(set_position_thread, "set_position_thread");
		DEBUG_INFO("create thread successfully");
	} else {
		DEBUG_INFO("Failed to create thread");
	}

	//  watch config.json  and update the layout of PCs
	pthread_t watch_config_thread;
	ret = pthread_create(&watch_config_thread, NULL, watch_config_thread_func, NULL);
	if (ret == 0) {
		DEBUG_INFO("create watch_config_thread successfully");
	} else {
		DEBUG_INFO("Failed to create watch_config_thread");
	}

	// TODO: 动态更新鼠标速度
	/* get mouse's speed */
	get_mouse_speed();

	// init epoll
	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		DEBUG_INFO("Failed to create epoll");
	} else {
		printf("create epoll successfully\n");
	}
	/* add keyboard and mouse into epollfd */
	add_kbd_mouse_to_epollfd();
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
		DEBUG_INFO("create watch_mouse_thread successfully");
	} else {
		DEBUG_INFO("Failed to create watch_mouse_thread");
	}

	///* deactivate mouse and keyboard */
	char* mouse_cmd = calloc(100, sizeof(char));
	char* kbd_cmd = calloc(100, sizeof(char));
	sprintf(mouse_cmd, "udevadm trigger --action=remove %s", mouse_dev_path);
	sprintf(kbd_cmd, "udevadm trigger --action=remove %s", kbd_dev_path);
	system(mouse_cmd);
	system(kbd_cmd);


	pthread_join(set_position_thread, NULL);
	pthread_join(watch_config_thread, NULL);
	pthread_join(send_event_thread, NULL);
	pthread_join(watch_mouse_thread, NULL);
	// 防止程序非ctrl + c 退出, 键盘鼠标不可用
	sprintf(mouse_cmd, "udevadm trigger --action=add %s", mouse_dev_path);
	sprintf(kbd_cmd, "udevadm trigger --action=add %s", kbd_dev_path);
	system(mouse_cmd);
	system(kbd_cmd);
	free(mouse_cmd);
	free(kbd_cmd);
	return 0;
}

