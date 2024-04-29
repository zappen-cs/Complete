# 赛题及要求
## 赛题：基于openKylin的hid input 设备共享协议
## (1) 赛题说明：

&emsp;&emsp;基于openKylin操作系统Wayland环境，实现多主机共享一套hid input设备的技术方案（以一套键鼠为例），形成一套具备安全性、通用性的协议标准。

## (2) 赛题要求

1. 支持一套键鼠控制多台主机（大于等于2）。鼠标可在多主机之间自由移动，键盘可跟随鼠标焦点所在的窗口做输入操作。多套主机在同一时刻仅有1套设备实际响应input操作；
2. 提供配置主机之间相对位置的功能，鼠标移动范围及方向受相对位置限制；
3. 支持主机间剪切板功能共享；
4. 额外功能：支持文本文档、图片、word文档等文件的主机间拖拽功能；

## (3) 赛题导师

liujie01@kylinos.cn

## (4) 参考资料

+ https://www.kernel.org/doc/html/latest/usb/usbip_protocol.html
+ https://help.ubuntu.com/community/SynergyHowto

# 键盘跨设备原理及实现

## 1.在键盘按下一个键，是如何显示在屏幕上的？

键盘被抽象成设备文件，应用程序负责与设备文件交互。

+ 键盘的行为，linux内核将其以event形式封装

```c
struct input_event {
    struct timeval time;//事件产生的时间戳
    __u16 type;// 事件类型，键盘设备应为按键事件；鼠标的点击也为按键事件，具体位置应为移动事件
    __u16 code;//事件码，用来标识键盘上的某个键
    __s32 value;//事件值，1表示按下，0表示抬起
};
```

+ 键盘产生数据，会往设备文件里写(write)
+ 应用程序读(read)设备文件，会获取键盘产生的数据

**参考资料：**

[What happens when a key is pressed](https://blog.dreamfever.me/posts/2022-01-04-what-happens-when-a-key-is-pressed-1/)

[键盘敲入A字母时，期间发生了什么....](https://juejin.cn/post/6864158680028774407)

## 2.跨终端键盘共享的方案设计

### 2.1 架构图

![](https://raw.githubusercontent.com/zappen-cs/myBlogResource/etc/imagepjc3.png)

### 2.2 方案说明

1. 键盘产生事件，上报至用户空间时，将其截获并转发
2. 在另一台终端设备上创建一个虚拟设备文件，接收键盘的事件并写入该文件

### 2.3 共享键盘使用说明

```c
// 情景：设备1存在键盘，设备2不存在键盘
1.设备1编译listen_kbd.c，设备2编译server.c;
2.设备2先运行server.c对应的可执行文件，随后设备1运行listen_kbd.c对应的可执行文件;
3.注意权限问题
```

# 进度

[04-29] 已经实现键盘的共享



# TODO

+ ~~键盘的共享~~
+ 鼠标的共享
+ 鼠标在多设备间自由移动
+ 键盘可随鼠标所在的窗口中做输入
+ 支持主机间剪切板功能共享
