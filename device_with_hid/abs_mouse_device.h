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

#ifndef _ABS_MOUSE_DEVICE_H
#define _ABS_MOUSE_DEVICE_H




// 创建一个绝对位置的鼠标
extern struct uinput_user_dev abs_mouse_dev;
// 绝对位置鼠标的一个句柄
extern int abs_mouse_fd;
// 鼠标全局位置
extern double global_x, global_y;
// 创建一个支持绝对坐标的鼠标
void create_abs_mouse();
// 设置鼠标位置
void set_mouse_position(double px, double py);

#endif // _ABS_MOUSE_DEVICE_H
