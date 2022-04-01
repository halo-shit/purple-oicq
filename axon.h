#ifndef AXON_H_INCLUDED
#define AXON_H_INCLUDED

/**
 * 通过 Socket，连接到 Axon 程序。
 *
 * @param host 要连接到的主机名
 * @param port 要连接主机的端口
 * @return 对应 Socket 的文件描述符
 */
int axon_connect(const char *, const char *);

/**
 * 初始化 Axon 客户端（OICQ 示例）。
 *
 * @param sockfd Socket 文件描述符
 * @param self_id 要登录帐号的 QQ 号
 */
void axon_client_init(int, const char *, const char *);

/**
 * 使 Axon 客户端登录到 QQ 服务器（密码登录）。
 *
 * @param sockfd Socket 文件描述符
 * @param passwd 密码（或 16 位 MD5 字符串）
 */
void axon_client_password_login(int, const char *);

/**
 * 使 Axon 客户端登录到 QQ 服务器（扫码登录）。
 *
 * @param sockfd Socket 文件描述符
 */
void axon_client_qrcode_login(int);

/**
 * 调用一条不含参数的命令。
 *
 * @param sockfd Socket 文件描述符
 * @param cmd 命令名称
 */
void axon_client_call(int, const char *);

/**
 * 向好友发送一个普通窗口抖动。
 *
 * @param sockfd Socket 文件描述符
 * @param nick 对象用户的替代昵称
 */
void axon_client_fsend_shake(int, const char *);

/**
 * 向好友发送一条文本消息。
 *
 * @param sockfd Socket 文件描述符
 * @param nick 对象用户的替代昵称
 * @param message 要发送的文本消息
 */
void axon_client_fsend_plain(int, const char *, const char *);

/**
 * 向群聊发送一条文本消息。
 *
 * @param sockfd Socket 文件描述符
 * @param id 对象群聊的群号
 * @param message 要发送的文本消息
 */
void axon_client_gsend_plain(int, const char *, const char *);

/**
 * 根据昵称查询用户信息。
 *
 * @param sockfd Socket 文件描述符
 * @param id 用户的替代昵称
 */
void axon_client_fetch_acct_info(int, const char *);

/**
 * 查询群聊的基本信息。
 *
 * @param sockfd Socket 文件描述符
 * @param id 要查询的群号
 */
void axon_client_fetch_group_info(int, const char *);

/**
 * 查询群聊的用户列表。
 *
 * @param sockfd Socket 文件描述符
 * @param id 要查询的群号
 */
void axon_client_fetch_group_members(int, const char *);

/**
 * 查询替代昵称所对应的 QQ 号。
 *
 * @param sockfd Socket 文件描述符
 * @param nick 要查询用户的替代昵称
 */
void axon_client_lookup_nickname(int, const char *);

/**
 * 修改 QQ 的在线状态。
 *
 * @param sockfd Socket 文件描述符
 * @param new_status 新状态
 */
void axon_client_update_status(int, const char *);

#endif
