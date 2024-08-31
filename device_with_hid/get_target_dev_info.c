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

char *message = NULL;
char left_dev_ip[IPLEN];
char right_dev_ip[IPLEN];

/* to get json data from xxx.json */
void get_json_from_file() {
	FILE *fp = fopen("config.json", "r");	
	if (fp == NULL) {
		perror("Failed to open file\n");
		exit(1);
	}
	fseek(fp, 0L, SEEK_END); // move cursor to the end of file
	int file_size = ftell(fp); // get the size of file
	message = (char *)malloc(file_size + 1);
	if (message == NULL) {
		perror("Failed to malloc\n");
		fclose(fp);
		exit(1);
	}
	fseek(fp, 0L, SEEK_SET); // move cursor to the begin of file
	fread(message, file_size, 1, fp); // read all data of file
	message[file_size] = '\0';
}

void get_info() {

	get_json_from_file();

	cJSON *cjson_data = NULL;
    cJSON *cjson_left_dev_ip = NULL;
    cJSON *cjson_right_dev_ip = NULL;


    cjson_data= cJSON_Parse(message); // parse data
    if(cjson_data== NULL) {
        printf("parse fail.\n");
		exit(1);
    }

    cjson_left_dev_ip = cJSON_GetObjectItem(cjson_data, "left-device-ip");
    cjson_right_dev_ip = cJSON_GetObjectItem(cjson_data, "right-device-ip");

	if (cjson_left_dev_ip) {
    	//DEBUG_INFO("left-device-ip: %s\n", cjson_left_dev_ip->valuestring);
		memset(left_dev_ip, 0, IPLEN);
		strcpy(left_dev_ip, cjson_left_dev_ip->valuestring);
	}

	if (cjson_right_dev_ip) {
    	//DEBUG_INFO("right-device-ip: %s\n", cjson_right_dev_ip->valuestring);
		memset(right_dev_ip, 0, IPLEN);
		strcpy(right_dev_ip, cjson_right_dev_ip->valuestring);
	}

}
