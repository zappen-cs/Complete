/*
 ***********************************************

 @Author:   zappen
 @Mail:     zappen@csu.edu.cn
 @Date:     2024-09-21
 @FileName: watch_config_json.c
 ***********************************************
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/inotify.h>

#define EVENT_NUM  12

const char *event_str[EVENT_NUM] = {
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


int watch_config_handler() {
	int errTimes = 0;
	int fd = -1;

INIT_INOTIFY:
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
		for(i = 0; i< EVENT_NUM; i++)
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

#if 1
int main() {
	if (watch_config_handler() == -1) {
		return -1;
	}

	return 0;
}
#endif
