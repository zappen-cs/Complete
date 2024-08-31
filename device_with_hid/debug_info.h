#ifndef _DEBUG_INFO_H
#define _DEBUG_INFO_H


#define DEBUG_OPEN
#ifdef DEBUG_OPEN
	#define DEBUG_INFO(format,...) printf("File: %s, Function: %s, Line: %d: "format"\n",__FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
	#define DEBUG_INFO(format,...)
#endif // DEBUG_OPEN

#endif // _DEBUG_INFO_H
