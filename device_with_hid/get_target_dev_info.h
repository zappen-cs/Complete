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
