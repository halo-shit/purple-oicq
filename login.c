#include "login.h"
#include "axon.h"
#include "common.h"
#include "debug.h"
#include "event.h"
#include "glibconfig.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

void axon_client_login_err (PurpleConnection *, gpointer, JsonReader *);

void
buddy_icon_download_cb (PurpleUtilFetchUrlData * _, gpointer dptr,
			const gchar * buffer, gsize len,
			const gchar * error_message)
{
  BUDDY_INFO	*binfo = dptr;
  guchar	*data  = g_malloc0 (len + 1);

  if (len == 0)
    {
      DEBUG_LOG ("failed to retrieve buddy icons");
      DEBUG_LOG (error_message);
      return;
    }

  memcpy (data, buffer, len);
  purple_buddy_icon_new (binfo->pc->account, binfo->name, data, len, NULL);

  g_free (binfo->name);
  g_free (binfo);
}

void
purple_update_buddy_icon (PurpleConnection * pc, int64_t id, char *name)
{
  gchar		 url[64];
  BUDDY_INFO	*binfo = g_malloc0 (sizeof (BUDDY_INFO));

  g_snprintf (url, 63, "http://q1.qlogo.cn/g?b=qq&nk=%zu&s=640", id);

  binfo->pc = pc;
  binfo->name = name;
  purple_util_fetch_url_len (url, TRUE, NULL, TRUE, MEDIA_MAX_LEN,
			     buddy_icon_download_cb, binfo);
}

void
purple_init_err (PurpleConnection * pc, gpointer message, JsonReader *_)
{
  purple_connection_error_reason (pc, PURPLE_CONNECTION_ERROR_OTHER_ERROR, message);
}

void
axon_client_glist_ok (PurpleConnection * pc, gpointer _, JsonReader *data)
{
  DEBUG_LOG ("sync groups");

  gint		 chat_count;
  GHashTable	*components;
  const gchar	*name, *id;
  PurpleGroup	*group;
  PurpleChat	*chat;

  json_reader_read_member (data, "idlist");
  chat_count = json_reader_count_elements (data);
  json_reader_end_member (data);
  /* 如果对应分组不存在就新建一个 */
  group = purple_find_group (PRPL_SYNC_GROUP_CHAT);
  if (group == NULL)
    {
      DEBUG_LOG ("group not found, will create one");
      group = purple_group_new (PRPL_SYNC_GROUP_CHAT);
      purple_blist_add_group (group, NULL);
    }
  else
    {
      goto skip;
    }
  /* 将群聊加入分组 */
  for (int i = 0; i < chat_count; i++)
    {
      json_reader_read_member (data, "namelist");
      json_reader_read_element_string (data, i, name);
      json_reader_end_member (data);
      
      json_reader_read_member (data, "idlist");
      json_reader_read_element_string (data, i, id);
      json_reader_end_member (data);

      components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
      g_hash_table_insert (components, PRPL_CHAT_INFOID, g_strdup (id));
      g_hash_table_insert (components, PRPL_CHAT_INFONAME, g_strdup (name));
      chat = purple_chat_new (pc->account, name, components);

      purple_blist_add_chat (chat, group, NULL);
    }

skip:{
  }
  purple_connection_set_state (pc, PURPLE_CONNECTED);
}

void
axon_client_flist_ok (PurpleConnection * pc, gpointer _, JsonReader *data)
{
  DEBUG_LOG ("sync friends");

  PD_FROM_PTR (pc->proto_data);
  gint		 friend_count;
  const gchar	*name;
  PurpleGroup	*group;
  PurpleBuddy	*buddy;

  json_reader_read_member (data, "list");
  friend_count = json_reader_count_elements (data);
  json_reader_end_member (data);
  /* 如果对应分组不存在就新建一个 */
  group = purple_find_group (PRPL_SYNC_GROUP_BUDDY);
  if (group == NULL)
    {
      group = purple_group_new (PRPL_SYNC_GROUP_BUDDY);
      purple_blist_add_group (group, NULL);
    }
  /* 将好友加入分组 */
  for (int i = 0; i < friend_count; i++)
    {
      json_reader_read_member (data, "list");
      json_reader_read_element_string (data, i, name);
      json_reader_end_member (data);
      /* 若用户已存在，则直接设置在线状态 */
    set_online_status:
      if (purple_find_buddy (pc->account, name) != NULL)
	{
	  purple_prpl_got_user_status (pc->account, name, "online", NULL);
	  continue;
	}
      /* 不显示 BabyQ */
      if (!strcmp (name, "babyQ"))
	continue;
      /* 添加好友 */
      buddy = purple_buddy_new (pc->account, name, name);
      purple_blist_add_buddy (buddy, NULL, group, NULL);
      /* 处理好友头像 */
      char *altname = NULL; gint64 id;
      altname = g_strdup (name);

      json_reader_read_member (data, "idlist");
      json_reader_read_element_int (data, i, id);
      json_reader_end_member (data);
      
      purple_update_buddy_icon (pc, id, altname);
      goto set_online_status;
    }

  NEW_WATCHER_W ();
  w->ok = axon_client_glist_ok;
  w->err = purple_init_err;
  w->data = "没能同步群聊列表";
  g_queue_push_tail (pd->queue, w);

  axon_client_call (pd->fd, "GLIST");
}

void
axon_client_whoami_ok (PurpleConnection * pc, gpointer _, JsonReader *response)
{
  PD_FROM_PTR (pc->proto_data);
  const gchar	*name;

  DEBUG_LOG ("whoami ok, login completed");
  json_reader_read_string (response, "name", name);

  pd->whoami = g_strdup (name);

  NEW_WATCHER_W ();
  w->data = "没能同步好友列表";
  w->ok = axon_client_flist_ok;
  w->err = purple_init_err;
  g_queue_push_tail (pd->queue, w);

  axon_client_call (pd->fd, "FLIST");
}

void
axon_client_login_ok (PurpleConnection * pc, gpointer _, JsonReader *__)
{
  /* 将单次扫码登录改为密码登录 */
  if (purple_strequal (purple_account_get_string (pc->account, PRPL_ACCT_OPT_LOGIN, PRPL_ACCT_OPT_USE_PASSWORD),
		       PRPL_ACCT_OPT_USE_QRCODE_ONCE))
    purple_account_set_string (pc->account, PRPL_ACCT_OPT_LOGIN, PRPL_ACCT_OPT_USE_PASSWORD);

  PD_FROM_PTR (pc->proto_data);

  DEBUG_LOG ("login ok, wating for next procedure");
  purple_connection_update_progress (pc, "查询身份", 2, 5);

  NEW_WATCHER_W ();
  w->data = "没能查询到身份";
  w->ok = axon_client_whoami_ok;
  w->err = purple_init_err;
  g_queue_push_tail (pd->queue, w);

  axon_client_call (pd->fd, "WHOAMI");
}

void
go_ahead_cb (void *dptr)
{
  PurpleConnection	*pc = dptr;
  PD_FROM_PTR (pc->proto_data);

  NEW_WATCHER_W ();
  w->ok = axon_client_login_ok;
  w->err = axon_client_login_err;
  g_queue_push_tail (pd->queue, w);

  axon_client_call (pd->fd, "GOAHEAD");
}

void
axon_client_login_err (PurpleConnection *pc, gpointer _, JsonReader *data)
{
  guchar	*dptr = NULL;
  gint		 login_t;
  FILE		*qrcode;
  const gchar	*message;
  gsize		 dlen = 0;

  json_reader_read_int (data, "login", login_t);

  switch (login_t)
    {
    case L_ERROR:
      json_reader_read_string (data, "message", message);
      purple_connection_error_reason (pc, PURPLE_CONNECTION_ERROR_OTHER_ERROR, message);
      break;
    case L_SLIDER:
      /*
         json_object_object_get_ex(data, "url", &tmp);
         sprintf(pd->buf, "本次登录需要滑块登录，验证码：%s。"
         "请在滑块完成后输入 ticket。",
         json_object_get_string(tmp));
         purple_request_input(pc, "登录验证", pd->buf, NULL, "", FALSE,
         FALSE, NULL, "提交", NULL, "取消", NULL,
         pd->acct, NULL, NULL, NULL);
       */
      purple_connection_error_reason (pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE,
				      "本次登录需要滑块登录，请先进行一次扫码登录以跳过。");
      break;
    case L_QRCODE:
      json_reader_read_string (data, "data", message);
      purple_notify_message (pc, PURPLE_NOTIFY_MSG_WARNING, "登录验证",
			     "本次登录需要扫描二维码，请前往弹出图片扫描，并在扫描后关闭本窗口。\n",
			     "如果图片没有弹出，请访问 /tmp/oicq-qrcode-verify.png。\n"
			     "如果二维码失效，请关闭窗口以再次获取。",
			     go_ahead_cb, pc);

      dptr = purple_base64_decode (message, &dlen);

      qrcode = fopen ("/tmp/oicq-qrcode-verify.png", "w");
      if (qrcode == NULL)
	{
	  DEBUG_LOG ("failed to open file");
	  return;
	}

      fwrite (dptr, 1, dlen, qrcode);
      fflush (qrcode);
      fclose (qrcode);

      system ("xdg-open /tmp/oicq-qrcode-verify.png &");
      break;
    case L_DEVICE:
      json_reader_read_string (data, "url", message);
      purple_notify_message (pc, PURPLE_NOTIFY_MSG_WARNING, "登录验证",
			     "本次登录需要解锁设备锁，请前往弹出链接解锁，并在完成后关闭此窗口提交。",
			     NULL, go_ahead_cb, pc);
      purple_notify_uri (pc, message);
      break;
    }
}

void
axon_client_init_ok (PurpleConnection * pc, gpointer _, JsonReader *__)
{
  PD_FROM_PTR (pc->proto_data);

  DEBUG_LOG ("init ok, wating for next procedure");
  purple_connection_update_progress (pc, "登录到 QQ", 2, 5);

  NEW_WATCHER_W ();
  w->ok = axon_client_login_ok;
  w->err = axon_client_login_err;
  g_queue_push_tail (pd->queue, w);

  /* 登录方式检查 */
  if (purple_strequal (purple_account_get_string (pc->account, PRPL_ACCT_OPT_LOGIN, PRPL_ACCT_OPT_USE_PASSWORD),
		       PRPL_ACCT_OPT_USE_PASSWORD))
    {
      axon_client_password_login (pd->fd, pc->account->password);
    }
  else
    {
      axon_client_qrcode_login (pd->fd);
    }
}
