#ifndef _OICQ_H_
#define _OICQ_H_

#include <glib.h>
#include <purple.h>

typedef struct json_object *Data;

/* “关于” 信息 */
#define DISPLAY_VERSION "0.0.2"
#define PRPL_WEBSITE "http://github.com"

/* 聊天信息 "components" 的标识 */
#define PRPL_CHAT_INFO_QQ_GID "group_id"

#define PRPL_ACCOUNT_OPT_LOGIN "login"
#define PRPL_ACCOUNT_OPT_HOST  "host"
#define PRPL_ACCOUNT_OPT_PORT  "port"
#define PRPL_ACCOUNT_OPT_TLS   "secure"

#define PRPL_ACCOUNT_OPT_USE_PASSWORD	 "password"
#define PRPL_ACCOUNT_OPT_USE_QRCODE	 "qrcode"
#define PRPL_ACCOUNT_OPT_USE_QRCODE_ONCE "qrcode-once"

#define PRPL_SYNC_GROUP "好友列表"


#endif // _OICQ_H_
