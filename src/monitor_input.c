#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <string.h>
#include <dirent.h>

/* 
    屏保配置文件，其中：
    enable=1
    timeout=60
    program=/home/uos/screen_clock2
    代表开启屏保，超时时间为60秒，超时后执行屏幕保护程序/home/uos/screen_clock2
*/
#define SCREEN_PROTECT_CONFIG "/opt/apps/zerrio-screensaver/conf/screen_protect.conf"

enum event_type {
    EVENT_TYPE_KEYBOARD = 1,
    EVENT_TYPE_MOUSE = 2,
};

struct input_event_device {
    enum event_type type;
    int fd;
    char *path;
    struct input_event_device *next;
};

struct input_event_device *input_event_device_head = NULL;
struct input_event_device *input_event_device_tail = NULL;
int event_device_count = 0;

// 根据fd遍历input_event_device链表，返回对应的input_event_device
struct input_event_device *get_input_event_device_by_fd(int fd)
{
    struct input_event_device *input_event_device_tmp = input_event_device_head;
    while (input_event_device_tmp != NULL) {
        if (input_event_device_tmp->fd == fd) {
            return input_event_device_tmp;
        }
        input_event_device_tmp = input_event_device_tmp->next;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int fd_keyboard, fd_mouse, ret;
    struct input_event data;
    unsigned int key;
    int timeout = 60 * 1000;

    // 打开输入设备文件
    DIR *dir;
    struct dirent *entry;
    struct input_event_device *input_event_device_tmp;
    char *kbd_path = NULL;
    char *mouse_path = NULL;

    // 读取配置文件
    FILE *fp;
    char line[1024];
    char *key_str, *value_str;
    char *program = NULL;
    int enable = 0;
    fp = fopen(SCREEN_PROTECT_CONFIG, "r");
    if (fp == NULL) {
        printf("无法打开配置文件\n");
        exit(1);
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        key_str = strtok(line, "=");
        value_str = strtok(NULL, "=");
        if (key_str != NULL && value_str != NULL) {
            if (strstr(key_str, "enable") != NULL) {
                if (strstr(value_str, "1") != NULL) {
                    enable = 1;
                } else {
                    enable = 0;
                }
            }
            if (strstr(key_str, "timeout") != NULL) {
                timeout = atoi(value_str) * 1000;
            }
            if (strstr(key_str, "program") != NULL) {
                program = malloc(strlen(value_str) + 1);
                strcpy(program, value_str);
            }
        }
    }
    fclose(fp);

    if (enable == 0 || program == NULL) {
        printf("屏保未开启\n");
        exit(0);
    }

    dir = opendir("/dev/input/by-id");
    if (dir == NULL) {
        printf("无法打开输入设备目录\n");
        exit(1);
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "event-kbd") != NULL) {
            kbd_path = malloc(strlen("/dev/input/by-id/") + strlen(entry->d_name) + 1);
            strcpy(kbd_path, "/dev/input/by-id/");
            strcat(kbd_path, entry->d_name);
            fd_keyboard = open(kbd_path, O_RDONLY | O_CLOEXEC);
            if (fd_keyboard == -1) {
                printf("无法打开键盘输入设备文件\n");
                exit(1);
            }
            printf("find event-kbd[%d]: %s\n", fd_keyboard, kbd_path);
            input_event_device_tmp = malloc(sizeof(struct input_event_device));
            if (input_event_device_tmp == NULL) {
                printf("无法分配内存\n");
                exit(1);
            }
            event_device_count++;
            input_event_device_tmp->type = EVENT_TYPE_KEYBOARD;
            input_event_device_tmp->fd = fd_keyboard;
            input_event_device_tmp->path = kbd_path;
            input_event_device_tmp->next = NULL;
            if (input_event_device_head == NULL) {
                input_event_device_head = input_event_device_tmp;
                input_event_device_tail = input_event_device_tmp;
            } else {
                input_event_device_tail->next = input_event_device_tmp;
                input_event_device_tail = input_event_device_tmp;
            }

        }
        if (strstr(entry->d_name, "event-mouse") != NULL) {
            mouse_path = malloc(strlen("/dev/input/by-id/") + strlen(entry->d_name) + 1);
            strcpy(mouse_path, "/dev/input/by-id/");
            strcat(mouse_path, entry->d_name);
            fd_mouse = open(mouse_path, O_RDONLY | O_CLOEXEC);
            if (fd_mouse == -1) {
                printf("无法打开鼠标输入设备文件\n");
                exit(1);
            }
            printf("find event-mouse[%d]: %s\n", fd_mouse, mouse_path);
            input_event_device_tmp = malloc(sizeof(struct input_event_device));
            if (input_event_device_tmp == NULL) {
                printf("无法分配内存\n");
                exit(1);
            }
            event_device_count++;
            input_event_device_tmp->type = EVENT_TYPE_MOUSE;
            input_event_device_tmp->fd = fd_mouse;
            input_event_device_tmp->path = mouse_path;
            input_event_device_tmp->next = NULL;
            if (input_event_device_head == NULL) {
                input_event_device_head = input_event_device_tmp;
                input_event_device_tail = input_event_device_tmp;
            } else {
                input_event_device_tail->next = input_event_device_tmp;
                input_event_device_tail = input_event_device_tmp;
            }
        }
    }
    closedir(dir);

    if (kbd_path == NULL && mouse_path == NULL) {
        printf("无法找到键盘或鼠标输入设备文件\n");
        exit(1);
    }

	uid_t uid = getuid();
	uid_t euid = geteuid();
	//printf("uid=%d, euid=%d\n", uid, euid);

	seteuid(uid);
	//uid = getuid();
	//euid = geteuid();
	//printf("after seteuid: uid=%d, euid=%d\n", uid, euid);

    printf("开始监听输入事件和鼠标移动事件...\n");

    struct pollfd *fds;
    fds = malloc(sizeof(struct pollfd) * event_device_count);
    if (fds == NULL) {
        printf("无法分配内存\n");
        exit(1);
    }
    int i = 0;
    input_event_device_tmp = input_event_device_head;
    while (input_event_device_tmp != NULL) {
        fds[i].fd = input_event_device_tmp->fd;
        fds[i].events = POLLIN;
        input_event_device_tmp = input_event_device_tmp->next;
        i++;
    }

    while (1) {
        ret = poll(fds, event_device_count, timeout);
        if (ret > 0) {
            // 有输入事件
            for (i = 0; i < event_device_count; i++) {
                if (fds[i].revents & POLLIN) {
                    read(fds[i].fd, &data, sizeof(data));
                }
            }
        } else if (ret == 0) {
            // 超时
            system(program);
        }
    }

    return 0;
}
