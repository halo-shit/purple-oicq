#ifndef _OICQ_H_
#define _OICQ_H_

#include <glib.h>
#include <purple.h>

struct oicq_conn {
	PurpleAccount *account;
	int fd;

	const char *whoami;
	char *inbuf;
};

/* “关于” 信息 */
#define DISPLAY_VERSION "0.0.1"
#define MATRIX_WEBSITE "http://github.com"

/* 协议 ID */
#define PRPL_ID "prpl-hammer-oicq"

/* 聊天信息 "components" 的标识 */
#define PRPL_CHAT_INFO_QQ_GID "group_id"

#define PRPL_ACCOUNT_OPT_HOST "host"
#define PRPL_ACCOUNT_OPT_PORT "port"
#define PRPL_ACCOUNT_OPT_TLS  "secure"

#endif // _OICQ_H_
