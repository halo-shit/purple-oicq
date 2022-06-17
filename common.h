#ifndef COMMON_H_GUARD
#define COMMON_H_GUARD

#include <glib.h>
#include <purple.h>
#include <json-glib/json-glib.h>

typedef struct
{
  PurpleAccount *acct;
  gint		 fd;
  gchar		*whoami;
  gchar		*buf;
  gsize		 buf_size;
  GQueue	*queue;
  JsonParser	*parser;
  JsonReader	*reader;
} ProtoData;


/* “关于” 信息 */
#define DISPLAY_VERSION "1.0.0"
#define PRPL_WEBSITE "https://github.com/axon-oicq/"

/* 聊天信息 "components" 的标识 */
#define PRPL_CHAT_INFOID   "group_id"
#define PRPL_CHAT_INFONAME "group_name"

#define PRPL_ACCT_OPT_LOGIN "login"
#define PRPL_ACCT_OPT_PROTO "proto"
#define PRPL_ACCT_OPT_HOST  "host"
#define PRPL_ACCT_OPT_PORT  "port"
#define PRPL_ACCT_OPT_TLS   "secure"

#define PRPL_ACCT_OPT_USE_PASSWORD	 "password"
#define PRPL_ACCT_OPT_USE_QRCODE	 "qrcode"
#define PRPL_ACCT_OPT_USE_QRCODE_ONCE "qrcode-once"

#define PRPL_SYNC_GROUP_BUDDY "好友列表"
#define PRPL_SYNC_GROUP_CHAT  "群聊列表"
#define PRPL_ID "purple-oicq"

#define DEBUG_LOG(x...) purple_debug_info(PRPL_ID, "%s(%d): %s\n", \
    __FUNCTION__, __LINE__, ## x)
#define PD_FROM_PTR(ptr) ProtoData *pd = ptr
#define NEW_WATCHER_W() Watcher *w = g_new(Watcher, 1)

/* Json 处理 */
#define json_reader_read_string(reader, key, s)	\
  json_reader_read_member (reader, key);	\
  s = json_reader_get_string_value (reader);	\
  json_reader_end_member (reader)

#define json_reader_read_element_string(reader, i, s)	\
  json_reader_read_element (reader, i);	\
  s = json_reader_get_string_value (reader);	\
  json_reader_end_element (reader)

#define json_reader_read_int(reader, key, v)	\
  json_reader_read_member (reader, key);	\
  v = json_reader_get_int_value (reader);	\
  json_reader_end_member (reader);

#define json_reader_read_element_int(reader, i, v)	\
  json_reader_read_element (reader, i);		\
  v = json_reader_get_int_value (reader);	\
  json_reader_end_element (reader)

#define json_reader_log_error(r)		\
  if (json_reader_get_error (r) != NULL)	\
    DEBUG_LOG (json_reader_get_error (r)->message)

/* 考虑到两千人群，缓冲区需要这么大 */
#define DEFAULT_BUFSIZE 8192

#endif
