#include "account.h"
#include "blist.h"
#include "connection.h"
#include "conversation.h"
#include "debug.h"
#include "event.h"
#include "eventloop.h"
#include "login.h"
#include "prpl.h"
#include "common.h"

#include "axon.h"
#include "chat.h"
#include "server.h"
#include "status.h"
#include "util.h"
#include <purple.h>
#include <unistd.h>


/* 获取账户图标 */
static const char *
prpl_list_icon (PurpleAccount *_, PurpleBuddy *__)
{
  return "oicq";
}

/* 用于获取一份装有对用户可用的状态的列表 */
static GList *
prpl_status_types (PurpleAccount *acct)
{
  GList			*types = NULL;
  PurpleStatusType	*type;

  /* 加入离线状态 */
  type = purple_status_type_new (PURPLE_STATUS_OFFLINE, "offline", "离线", TRUE);
  types = g_list_prepend (types, type);
  /* 加入在线状态 */
  type = purple_status_type_new (PURPLE_STATUS_AVAILABLE, "online", "在线", TRUE);
  types = g_list_prepend (types, type);
  /* 加入忙碌状态 */
  type = purple_status_type_new (PURPLE_STATUS_AWAY, "busy", "忙碌", TRUE);
  types = g_list_prepend (types, type);
  /* 加入隐身状态 */
  type = purple_status_type_new (PURPLE_STATUS_INVISIBLE, "invisible", "隐身", TRUE);
  types = g_list_prepend (types, type);

  return types;
}

/* 获取需要填写的群聊信息 */
static GList *
prpl_chat_info (PurpleConnection *pc)
{
  GList				*params = NULL;
  struct proto_chat_entry	*pce;

  pce = g_new0 (struct proto_chat_entry, 1);
  pce->label = "群名:";	/* 需要本地化 */
  pce->identifier = PRPL_CHAT_INFONAME;
  pce->required = FALSE;
  params = g_list_append (params, pce);

  pce = g_new0 (struct proto_chat_entry, 1);
  pce->label = "群号:";	/* 需要本地化 */
  pce->identifier = PRPL_CHAT_INFOID;
  pce->required = TRUE;
  params = g_list_append (params, pce);

  return params;
}

/* 默认填写的群聊信息 [WIP] */
static GHashTable *
prpl_chat_info_defaults (PurpleConnection *_, const char *__)
{
  GHashTable	*defaults;
  defaults = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
  return defaults;
}

/* 关闭协议 */
static void
prpl_close (PurpleConnection *pc)
{
  if (pc->proto_data == NULL)
    return;

  PD_FROM_PTR (pc->proto_data);

  DEBUG_LOG ("closing axon connection");
  purple_input_remove (pc->inpa);
  close (pd->fd);

  DEBUG_LOG ("freeing resources");
  g_free (pd->buf);
  g_queue_free (pd->queue);
  g_free (pd->whoami);
  g_free (pd);
}

/* 登录帐号 */
static void
prpl_login (PurpleAccount *acct)
{
  gint			 sock_conn = -1;
  ProtoData		*pd	   = g_new0 (ProtoData, 1);
  PurpleConnection	*pc	   = acct->gc;
  
  purple_connection_set_state (pc, PURPLE_CONNECTING);

  DEBUG_LOG ("connecting to axon");
  purple_connection_update_progress (pc, "连接到 AXON", 0, 5);
  /* 打开到 AXON 的 Socket 连接 */
  sock_conn = axon_connect (purple_account_get_string (acct,PRPL_ACCT_OPT_HOST, "localhost"),
			    purple_account_get_string (acct, PRPL_ACCT_OPT_PORT, "9999"));
  if (sock_conn < 0)
    {
      purple_connection_error_reason (pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "没能连接到 AXON 服务端");
      return;
    }

  DEBUG_LOG ("loading protocol data");
  pd->acct = acct;
  pd->fd = sock_conn;
  pd->buf = g_malloc0 (BUFSIZE);
  pd->queue = g_queue_new ();
  purple_connection_set_protocol_data (pc, pd);

  DEBUG_LOG ("waiting for next procedure");
  purple_connection_update_progress (pc, "初始化客户端", 1, 5);

  NEW_WATCHER_W ();
  w->data = "没能初始化 OICQ 客户端";
  w->ok = axon_client_init_ok;
  w->err = purple_init_err;
  g_queue_push_tail (pd->queue, w);

  pc->inpa = purple_input_add (pd->fd, PURPLE_INPUT_READ, event_cb, pd);
  axon_client_init (pd->fd, pc->account->username,
		    purple_account_get_string (acct, PRPL_ACCT_OPT_PROTO, "0"));
}

static char *
prpl_get_cb_real_name (PurpleConnection *gc, gint id, const gchar *who)
{
  return NULL;
}

/* 初始化窗口抖动 */
static GList *
prpl_attention_types (PurpleAccount *acct)
{
  PurpleAttentionType	*pat;
  pat = purple_attention_type_new ("shake", "shake",
				   "您收到了一个窗口抖动！",
				   "您发送了一个窗口抖动！");
  return g_list_append (NULL, pat);
}

/* 发送私聊消息 */
static int
prpl_im_send (PurpleConnection *pc, const gchar *who,
	      const gchar *message, PurpleMessageFlags flags)
{
  const char		*image_start, *image_end;
  GData			*image_attribs;
  PurpleConversation	*conv;

  conv = purple_find_conversation_with_account (PURPLE_CONV_TYPE_IM, who, pc->account);

  if (purple_markup_find_tag ("img", message, &image_start, &image_end, &image_attribs))
    {
      int imgstore_id = atoi (g_datalist_get_data (&image_attribs, "id"));
      purple_imgstore_ref_by_id (imgstore_id);

      u2u_img_message_send (pc, conv, imgstore_id);

      g_datalist_clear (&image_attribs);
      return 0;
    }

  u2u_message_send (pc, conv, message);

  return 0;
}

/* 发送群聊消息 */
static int
prpl_chat_send (PurpleConnection *pc, gint id,
		const gchar *message, PurpleMessageFlags flags)
{
  PurpleConversation	*conv = purple_find_chat (pc, id);
  const char		*image_start, *image_end;
  GData			*image_attribs;
  
  if (conv == NULL)
      return -1;

  if (purple_markup_find_tag ("img", message, &image_start, &image_end, &image_attribs))
    {
      int imgstore_id = atoi (g_datalist_get_data (&image_attribs, "id"));
      purple_imgstore_ref_by_id (imgstore_id);

      u2c_img_message_send (pc, conv, imgstore_id);

      g_datalist_clear (&image_attribs);
      return 0;
    }

  u2c_message_send (pc, conv, message);

  return 0;
}

/* 发送窗口抖动 */
static gboolean
prpl_send_attention (PurpleConnection *pc, const gchar *who, guint type)
{
  PD_FROM_PTR (pc->proto_data);
  axon_client_fsend_shake (pd->fd, who);
  g_queue_push_tail (pd->queue, watcher_nil ());
  return TRUE;
}

/* 加入（现有）群聊 */
static void
prpl_join_chat (PurpleConnection *pc, GHashTable *components)
{
  PurpleConversation *conv;
  const char *s_id = g_hash_table_lookup (components, PRPL_CHAT_INFOID);
  const char *s_name = g_hash_table_lookup (components, PRPL_CHAT_INFONAME);

  g_assert (s_id != NULL);

  if (s_name == NULL)
      return;
  
  conv = purple_find_chat (pc, atoi (s_id));
  if (conv == NULL)
    {
      conv = serv_got_joined_chat (pc, atoi (s_id), s_name);
      update_chat_members (pc, conv);
    }

  // PURPLE_CONV_CHAT(conv)->left = FALSE;
  // purple_conversation_update(conv, PURPLE_CONV_UPDATE_CHATLEFT);
}

static void
prpl_set_status (PurpleAccount *acct, PurpleStatus *status)
{
  DEBUG_LOG ("update status");
  PD_FROM_PTR (acct->gc->proto_data);
  const char *id = purple_status_get_id (status);
  /* 忽略离线状态 */
  if (purple_strequal (id, "offline"))
    return;
  /* 发送对应的 ID */
  axon_client_update_status (pd->fd, id);
  g_queue_push_tail (pd->queue, watcher_nil ());
}

void
prpl_get_info (PurpleConnection *pc, const gchar *who)
{
  DEBUG_LOG ("lookup user");
  PD_FROM_PTR (pc->proto_data);
  NEW_WATCHER_W ();

  w->data = (gpointer) who;
  w->ok = lookup_ok;
  w->err = lookup_err;
  g_queue_push_tail (pd->queue, w);
  axon_client_lookup_nickname (pd->fd, who);
}

/* 协议描述 */
static PurplePluginProtocolInfo prpl_info = {
  OPT_PROTO_UNIQUE_CHATNAME | OPT_PROTO_CHAT_TOPIC |	/* 具有聊天主题（群/好友 描述/签名） */
    OPT_PROTO_IM_IMAGE,		/* 聊天支持图像（图片/表情） */
  NULL,				/* 用户名分割, 稍后在 prpl_init() 中初始化 */
  NULL,				/* 协议选项，稍后在 prpl_init() 中初始化 */
  {				/* PurpleBuddyIconSpec (好友头像) */
   "png,jpg,gif",		/* 格式 */
   0,				/* 最小宽度 */
   0,				/* 最小高度 */
   128,				/* 最大宽度 */
   128,				/* 最大高度 */
   10000,			/* 最大文件大小 */
   PURPLE_ICON_SCALE_DISPLAY,	/* 伸缩规则 */
   },
  prpl_list_icon,		/* 列表图标 */
  NULL,				/* 列表象征 */
  NULL,				/* 状态信息 */
  NULL,				/* 提示信息 */
  prpl_status_types,		/* 状态类型 */
  NULL,				/* 好友列表节点的右键菜单 */
  prpl_chat_info,		/* 聊天信息 */
  prpl_chat_info_defaults,	/* 默认聊天信息 */
  prpl_login,			/* 登录 */
  prpl_close,			/* 关闭 */
  prpl_im_send,			/* 发送私聊消息 */
  NULL,				/* 设置信息 */
  NULL,				/* 发送正在输入 */
  prpl_get_info,		/* 获取信息 */
  prpl_set_status,		/* 设置状态 */
  NULL,				/* 设置Idle */
  NULL,				/* 修改密码 */
  NULL,
  NULL,				/* 加好友 */
  NULL,				/* 删好友 */
  NULL,				/* 删好友 */
  NULL,				/* 设置允许 */
  NULL,				/* 设置阻止 */
  NULL,				/* 重设允许 */
  NULL,				/* 重设阻止 */
  NULL,				/* 设置允许/阻止 */
  prpl_join_chat,		/* 加入聊天 */
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,				/* ？？？ */
  prpl_chat_send,		/* 发送聊天 */
  NULL,				/* keepalive */
  NULL,				/* 注册用户 */
  NULL,				/* 已废弃 */
  NULL,				/* 已废弃 */
  NULL,				/* 好友别名 */
  NULL,				/* 好友分组 */
  NULL,				/* 重命名组 */
  NULL,				/* 删除好友 */
  NULL,				/* 关闭的群聊 */
  NULL,				/* 普通化 */
  NULL,				/* 设置好友图标 */
  NULL,				/* 删除群组 */
  prpl_get_cb_real_name,	/* 获取实名 */
  NULL,				/* 设置聊天话题 */
  NULL,				/* 查找好友列表聊天 */
  NULL,				/* 获取房间列表 */
  NULL,				/* 取消房间列表 */
  NULL,				/* 房间列表扩展分类 */
  NULL,				/* 接收文件 */
  NULL,				/* 发送文件 */
  NULL,				/* 新 Xfer */
  NULL,				/* 离线消息 */
  NULL,				/* ？？？ */
  NULL,				/* RAW 发送 */
  NULL,				/* 房间列表房间序列化 */
  NULL,				/* 注销用户 */
  prpl_send_attention,		/* 发送Attention */
  prpl_attention_types,		/* 获取Attention类别 */
  sizeof (PurplePluginProtocolInfo),	/* 结构体大小 */
  NULL,				/* 获取用户文本表 */
  NULL,				/* initiate_media */
  NULL,				/* get_media_caps */
  NULL,				/* 获取Moods */
  NULL,				/* 设置公共别名 */
  NULL,				/* 获取公共别名 */
  NULL,				/* 加好友，附带邀请 */
  NULL				/* 加好友，附带邀请 */
};

static PurplePluginInfo info = {
  PURPLE_PLUGIN_MAGIC,
  PURPLE_MAJOR_VERSION,
  PURPLE_MINOR_VERSION,
  PURPLE_PLUGIN_PROTOCOL,
  NULL,
  0,
  NULL,
  PURPLE_PRIORITY_DEFAULT,

  "prpl-hammer-oicq",
  "OICQ",
  DISPLAY_VERSION,

  "A binding to takayama-lily's OICQ library.",
  "A binding to takayama-lily's OICQ library."
    "Require a OICQ daemon to function correctly.",
  "axon-oicq@riseup.net",
  PRPL_WEBSITE,

  NULL,
  NULL,
  NULL,

  NULL,
  &prpl_info,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* 不是我不想优化，而是我懒 */
static void
prpl_init (PurplePlugin *plugin)
{
  /* 初始化协议选项 */
  struct _PurpleKeyValuePair *pair0, *pair1, *pair2, *pair3, *pair4;
  GList *login_options = NULL;
  GList *protocol_options = NULL;
  GList *login_proto_opts = NULL;

  pair0 = g_new0 (struct _PurpleKeyValuePair, 1);
  pair1 = g_new0 (struct _PurpleKeyValuePair, 1);
  pair2 = g_new0 (struct _PurpleKeyValuePair, 1);

  pair0->key = "密码登录";
  pair0->value = PRPL_ACCT_OPT_USE_PASSWORD;
  pair1->key = "扫码登录";
  pair1->value = PRPL_ACCT_OPT_USE_QRCODE;
  pair2->key = "扫码登录（单次）";
  pair2->value = PRPL_ACCT_OPT_USE_QRCODE_ONCE;

  login_options = g_list_append (login_options, pair0);
  login_options = g_list_append (login_options, pair1);
  login_options = g_list_append (login_options, pair2);

  pair0 = g_new0 (struct _PurpleKeyValuePair, 1);
  pair1 = g_new0 (struct _PurpleKeyValuePair, 1);
  pair2 = g_new0 (struct _PurpleKeyValuePair, 1);
  pair3 = g_new0 (struct _PurpleKeyValuePair, 1);
  pair4 = g_new0 (struct _PurpleKeyValuePair, 1);

  /* 1:安卓手机(默认) 2:aPad 3:安卓手表 4:MacOS 5:iPad */
  pair0->key = "安卓手机（和已登录手机冲突）";
  pair0->value = "1";
  pair1->key = "aPad";
  pair1->value = "2";
  pair2->key = "安卓手表";
  pair2->value = "3";
  pair3->key = "MacOS";
  pair3->value = "4";
  pair4->key = "iPad";
  pair4->value = "5";

  login_proto_opts = g_list_append (login_proto_opts, pair0);
  login_proto_opts = g_list_append (login_proto_opts, pair1);
  login_proto_opts = g_list_append (login_proto_opts, pair2);
  login_proto_opts = g_list_append (login_proto_opts, pair3);
  login_proto_opts = g_list_append (login_proto_opts, pair4);

  protocol_options = g_list_append (protocol_options,
				    purple_account_option_list_new("登录方式", PRPL_ACCT_OPT_LOGIN, login_options));
  protocol_options =
    g_list_append (protocol_options,
		   purple_account_option_list_new ("协议类型", PRPL_ACCT_OPT_PROTO, login_proto_opts));
  protocol_options =
    g_list_append (protocol_options,
		   purple_account_option_string_new ("服务器主机名", PRPL_ACCT_OPT_HOST, "127.0.0.1"));
  protocol_options =
    g_list_append (protocol_options,
		   purple_account_option_string_new ("服务器端口号", PRPL_ACCT_OPT_PORT, "9999"));

  prpl_info.protocol_options = protocol_options;
}

PURPLE_INIT_PLUGIN (oicq, prpl_init, info)
