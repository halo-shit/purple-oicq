#include "dconn.h"

#include <json-c/json.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * 初始化 Socket 并连接到 OICQ
 *
 * @returns Socket 连接的结果
 */
int
oicq_connect(int* sockfd, const char* host, const char* port)
{
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in oicq;
    oicq.sin_addr.s_addr = inet_addr(host);
	oicq.sin_family = AF_INET;
	oicq.sin_port = htons( atoi(port) );

    return connect(*sockfd , (struct sockaddr *)&oicq , sizeof(oicq));
}

/**
 * 初始化 OICQ 客户端
 *
 * @param uid OICQ 将要登录的用户 ID
 * @returns 初始化结果
 */
int
oicq_init(int *sockfd, const char* uin)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("INIT"));
    json_object_object_add(json_root,
                           "uin",
                           json_object_new_string(uin));

    const char* finalCommand = json_object_to_json_string(json_root);
    return write(*sockfd, finalCommand, strlen(finalCommand));
}

/**
 * 登录到 QQ 服务器
 *
 * @param passwd 密码明文或 MD5
 * @returns 登录结果
 */
int
oicq_login(int *sockfd, const char* passwd)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("LOGIN"));
    json_object_object_add(json_root,
                           "passwd",
                           json_object_new_string(passwd));

    const char* finalCommand = json_object_to_json_string(json_root);
    return write(*sockfd, finalCommand, strlen(finalCommand));
}

/**
 * 发送一条不带参数的命令，对于有返回的命令需要手动读取。
 *
 * @param command 将要发送的命令
 */
void
send_command(int sockfd, const char* command)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string(command));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(sockfd, finalCommand, strlen(finalCommand));
}

/**
 * 获取目前 OICQ 客户端的状态
 *
 * 0: 未初始化 1: 未登录 2: 已登录
 *
 * @returns 整数，代表客户端状态
 */
int
oicq_state(int *sockfd)
{
    char buffer[256];
    struct json_object *state;
    struct json_object *json_root;

    send_command(*sockfd, "STATE");

    read(*sockfd, buffer, 255);
    json_root = json_tokener_parse(buffer);
    json_object_object_get_ex(json_root, "state", &state);

    return json_object_get_int(state);
}

/**
 * 发送一条好友消息
 */
void
send_friend_message(int sockfd, const char *uid, struct json_object *message)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("USEND"));
    json_object_object_add(json_root,
                           "uid",
                           json_object_new_string(uid));
    json_object_object_add(json_root,
                           "message",
                           message);

    const char* finalCommand = json_object_to_json_string(json_root);
    write(sockfd, finalCommand, strlen(finalCommand));
}

/**
 * 发送一条纯文本好友消息
 */
void
send_friend_plain_message(int sockfd, const char *uid, const char *message)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("USEND"));
    json_object_object_add(json_root,
                           "uid",
                           json_object_new_string(uid));
    json_object_object_add(json_root,
                           "message",
                           json_object_new_string(message));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(sockfd, finalCommand, strlen(finalCommand));
}


/**
 * 发送一条群聊消息
 */
void
send_group_message(int sockfd, const char *gid, struct json_object *message)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("GSEND"));
    json_object_object_add(json_root,
                           "gid",
                           json_object_new_string(gid));
    json_object_object_add(json_root,
                           "message",
                           message);

    const char* finalCommand = json_object_to_json_string(json_root);
    write(sockfd, finalCommand, strlen(finalCommand));
}

/**
 * 发送一条纯文本群聊消息
 */
void
send_group_plain_message(int sockfd, const char *gid, const char *message)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("GSEND"));
    json_object_object_add(json_root,
                           "gid",
                           json_object_new_string(gid));
    json_object_object_add(json_root,
                           "message",
                           json_object_new_string(message));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(sockfd, finalCommand, strlen(finalCommand));
}


/**
 * 发送好友信息请求。
 *
 * @param fd Socket 的文件标识符
 * @param id 不带标识符的 QQ 号
 */
void
send_user_info_req(int fd, const char *uid)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("UINFO"));
    json_object_object_add(json_root,
                           "uid",
                           json_object_new_string(uid));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(fd, finalCommand, strlen(finalCommand));
}

/**
 * 发送群聊信息请求。
 *
 * @param fd Socket 的文件标识符
 * @param id 不带标识符的 QQ 群号
 */
void
send_group_info_req(int fd, const char *id)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("GINFO"));
    json_object_object_add(json_root,
                           "id",
                           json_object_new_string(id));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(fd, finalCommand, strlen(finalCommand));
}

/**
 * 发送群聊成员列表请求。
 *
 * @param fd Socket 的文件标识符
 * @param id 不带标识符的 QQ 群号
 */
void
send_group_members_req(int fd, const char *id)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("GMLIST"));
    json_object_object_add(json_root,
                           "id",
                           json_object_new_string(id));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(fd, finalCommand, strlen(finalCommand));
}

/**
 * 发送群聊成员查询请求。
 *
 * @param fd Socket 的文件标识符
 * @param id 不带标识符的 QQ 群号
 * @param name 要查询目标的昵称
 */
void
send_group_member_lookup(int fd, const char *id, const char *name)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("GULOOKUP"));
    json_object_object_add(json_root,
                           "id",
                           json_object_new_string(id));
    json_object_object_add(json_root,
                           "nickname",
                           json_object_new_string(name));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(fd, finalCommand, strlen(finalCommand));
}

/**
 * 发送好友查询请求。
 *
 * @param fd Socket 的文件标识符
 * @param name 要查询目标的昵称
 */
void
send_friend_lookup(int fd, const char *name)
{
    struct json_object *json_root;
    json_root = json_object_new_object();
    json_object_object_add(json_root,
                           "command",
                           json_object_new_string("ULOOKUP"));
    json_object_object_add(json_root,
                           "nickname",
                           json_object_new_string(name));

    const char* finalCommand = json_object_to_json_string(json_root);
    write(fd, finalCommand, strlen(finalCommand));
}
