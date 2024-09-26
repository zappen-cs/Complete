/*
 ***********************************************

@Author:   zappen
@Mail:     zp935675863@gmail.com
@Date:     2024-08-03
@FileName: demo.c
 ***********************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "get_target_dev_info.h"
#include "debug_info.h"
#include "cJSON.h"
#include "base.h"

char *json_data = NULL;
char up_dev_ip[IPLEN];
char down_dev_ip[IPLEN];
char left_dev_ip[IPLEN];
char right_dev_ip[IPLEN];

int up_dev_enable;
int down_dev_enable;
int left_dev_enable;
int right_dev_enable;

int up_dev_closed;
int down_dev_closed;
int left_dev_closed;
int right_dev_closed;

struct servant_device config_infos[4];
 

/* to get json data from xxx.json */
void get_json_data_from_file() {
	FILE *fp = fopen("config.json", "r");	
	if (fp == NULL) {
		perror("Failed to open file\n");
		exit(1);
	}
	fseek(fp, 0L, SEEK_END); // move cursor to the end of file
	int file_size = ftell(fp); // get the size of file
	json_data = (char *)malloc(file_size + 1);
	if (json_data == NULL) {
		perror("Failed to malloc\n");
		fclose(fp);
		exit(1);
	}
	fseek(fp, 0L, SEEK_SET); // move cursor to the begin of file
	fread(json_data, file_size, 1, fp); // read all data of file
	json_data[file_size] = '\0';
}

void get_servant_devices_info(char ***device_ip_list, char ***device_pos_list, int *device_num, char ***removed_device_ip_list, int *removed_device_num) {
	get_json_data_from_file();
    cJSON *json = cJSON_Parse(json_data);
    if (json == NULL) {
        printf("JSON parsing error\n");
        return ;
    }

    // 获取 IP 列表
    cJSON *json_ip_list = cJSON_GetObjectItem(json, "ip_list");
    cJSON *json_device_info = cJSON_GetObjectItem(json, "device_info");
	cJSON *json_removed_ip_list = cJSON_GetObjectItem(json, "removed_device_ip");

	*device_num = cJSON_GetArraySize(json_ip_list);
	*removed_device_num = cJSON_GetArraySize(json_removed_ip_list);

	*device_ip_list = malloc(*device_num * sizeof(char *));
	*device_pos_list = malloc(*device_num * sizeof(char *));
	*removed_device_ip_list = malloc(*removed_device_num * sizeof(char *));

    // 遍历 IP 列表
    for (int i = 0; i < *device_num; i++) {
        cJSON *ip_item = cJSON_GetArrayItem(json_ip_list, i);
        char *ip = ip_item->valuestring;
		(*device_ip_list)[i] = strdup(ip);
		//*device_ip_list[i] = ip;

        // 获取设备信息
        cJSON *info = cJSON_GetObjectItem(json_device_info, ip);
        if (info) {
			char *pos = cJSON_GetObjectItem(info, "position")->valuestring;
			(*device_pos_list)[i] = strdup(pos);
        }
    }
	for (int i = 0; i < *removed_device_num; i++) {
		cJSON *ip_item = cJSON_GetArrayItem(json_removed_ip_list, i);
		char *ip = ip_item->valuestring;
		(*removed_device_ip_list)[i] = strdup(ip);
	}
	for (int i = 0; i < *device_num; i++) {
		printf("device_ip: %s, position is %s\n", (*device_ip_list)[i], (*device_pos_list)[i]);
	}
	for (int i = 0; i < *removed_device_num; i++) {
		printf("removed_device_ip : %s\n", (*removed_device_ip_list)[i]);
	}

    // 释放 JSON 对象
    cJSON_Delete(json);
    return ;

}
