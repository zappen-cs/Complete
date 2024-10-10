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

#ifndef _DEBUG_INFO_H
#define _DEBUG_INFO_H


#define DEBUG_OPEN
#ifdef DEBUG_OPEN
	#define DEBUG_INFO(format,...) printf("File: %s, Function: %s, Line: %d: "format"\n",__FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
	#define DEBUG_INFO(format,...)
#endif // DEBUG_OPEN

#endif // _DEBUG_INFO_H
