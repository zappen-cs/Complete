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

#ifndef _GET_TARGET_DEV_INFO_H
#define _GET_TARGET_DEV_INFO_H

#include "base.h"
extern char left_dev_ip[IPLEN];
extern char right_dev_ip[IPLEN];
extern char up_dev_ip[IPLEN];
extern char down_dev_ip[IPLEN];

extern int right_dev_enable;
extern int left_dev_enable;
extern int up_dev_enable;
extern int down_dev_enable;

//void get_info();
void get_servant_devices_info(char ***, char ***, int *, char ***, int *);

#endif // _GET_TARGET_DEV_INFO_H
