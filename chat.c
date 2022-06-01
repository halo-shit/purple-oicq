#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <purple.h>

#include "axon.h"
#include "blist.h"
#include "common.h"
#include "conversation.h"
#include "debug.h"
#include "event.h"
#include "eventloop.h"
#include "glibconfig.h"
#include "imgstore.h"
#include "json_object.h"
#include "notify.h"
#include "server.h"
#include "chat.h"
#include "util.h"

struct message
{
  PurpleConversation	*conv;
  ProtoData		*pd;
  gchar			*text;
};

struct image_message
{
  PurpleConversation	*conv;
  ProtoData		*pd;
  gint			 image_id;
};

void
lookup_ok (PurpleConnection *pc, gpointer who, Data data)
{
  PurpleNotifyUserInfo	*user_info = purple_notify_user_info_new ();
  Data			 field;

  json_object_object_get_ex (data, "relation", &field);
  purple_notify_user_info_prepend_pair (user_info, "加入的群聊",
					json_object_get_string (field));

  json_object_object_get_ex (data, "sex", &field);
  purple_notify_user_info_prepend_pair (user_info, "性别",
					json_object_get_string (field));

  json_object_object_get_ex (data, "card", &field);
  purple_notify_user_info_prepend_pair (user_info, "群名片(昵称)",
					json_object_get_string (field));

  json_object_object_get_ex (data, "id", &field);
  purple_notify_user_info_prepend_pair (user_info, "帐号",
					json_object_get_string (field));

  json_object_object_get_ex (data, "nickname", &field);
  purple_notify_user_info_prepend_pair (user_info, "昵称",
					json_object_get_string (field));

  purple_notify_userinfo (pc, who, user_info, NULL, NULL);
  g_free (user_info);
}

void
lookup_err (PurpleConnection *pc, gpointer _, Data __)
{
  purple_notify_error (pc, "错误", "用户信息查询失败！请检查程序日志。", NULL);
}

void
u2c_message_ok (PurpleConnection *pc, gpointer data, Data _)
{
  struct message *d = data;

  DEBUG_LOG ("group message ok");

  purple_conv_chat_write (PURPLE_CONV_CHAT (d->conv),
			  d->pd->whoami, d->text,
			  PURPLE_MESSAGE_SEND,
			  g_get_real_time () / 1000 / 1000);

  g_free (d->text);
  g_free (d);
}

void
u2c_message_err (PurpleConnection *pc, gpointer data, Data _)
{
  struct message *d = data;

  purple_conv_chat_write (PURPLE_CONV_CHAT (d->conv),
			  "Axon", "发送失败，请检查连接。",
			  PURPLE_MESSAGE_ERROR,
			  g_get_real_time () / 1000 / 1000);

  g_free (d->text);
  g_free (d);
}

void
u2c_message_send (PurpleConnection *pc, PurpleConversation *conv,
		  const gchar *message)
{
  PD_FROM_PTR (pc->proto_data);
  struct message	*d = g_new0 (struct message, 1);
  gchar			*unescaped_msg, *original_msg, s_id[12];

  /* 原始的消息会被释放，所以得到一份拷贝。 */
  unescaped_msg = g_strdup (purple_unescape_html (message));
  original_msg	= g_strdup (message);

  d->conv = conv;
  d->text = original_msg;
  d->pd	  = pd;

  NEW_WATCHER_W ();
  w->ok	  = u2c_message_ok;
  w->err  = u2c_message_err;
  w->data = d;
  g_queue_push_tail (pd->queue, w);

  sprintf (s_id, "%d", purple_conv_chat_get_id (PURPLE_CONV_CHAT (conv)));
  axon_client_gsend_plain (pd->fd, s_id, unescaped_msg);
  g_free (unescaped_msg);
}

void
u2c_img_message_send (PurpleConnection *pc, PurpleConversation *conv,
		      gint id)
{
  struct message	*d	  = g_new0 (struct message, 1);
  PurpleStoredImage	*image;
  gchar			*b64_dptr = NULL;
  const guchar		*raw_dptr;

  PD_FROM_PTR (pc->proto_data);
  NEW_WATCHER_W ();

  image = purple_imgstore_find_by_id (id);
  if (image == NULL)
    return;
  
  raw_dptr = purple_imgstore_get_data (image);
  b64_dptr = purple_base64_encode (raw_dptr, purple_imgstore_get_size (image));

  d->text = g_strdup_printf ("\n<IMG ID=\"%d\">", id);
  d->conv = conv;
  d->pd	  = pd;

  w->ok = u2c_message_ok;
  w->err = u2c_message_err;
  w->data = d;
  g_queue_push_tail (pd->queue, w);

  gchar s_id[12];
  sprintf (s_id, "%d", purple_conv_chat_get_id (PURPLE_CONV_CHAT (conv)));
  axon_client_gsend_image (pd->fd, s_id, b64_dptr);
  g_free (b64_dptr);
}

void
u2u_message_ok (PurpleConnection *pc, gpointer data, Data _)
{
  struct message *d = data;

  purple_conv_im_write (PURPLE_CONV_IM (d->conv),
			d->pd->whoami,
			d->text,
			PURPLE_MESSAGE_SEND,
			g_get_real_time () / 1000 / 1000);

  g_free (d->text);
  g_free (d);
}

void
u2u_message_err (PurpleConnection *pc, gpointer data, Data _)
{
  struct message *d = data;

  purple_conv_im_write (PURPLE_CONV_IM (d->conv),
			"Axon", "发送失败，请检查连接。",
			PURPLE_MESSAGE_ERROR,
			g_get_real_time () / 1000 / 1000);

  g_free (d->text);
  g_free (d);
}

void
u2u_message_send (PurpleConnection *pc, PurpleConversation *conv,
		  const gchar *message)
{
  PD_FROM_PTR (pc->proto_data);
  struct message *d = g_new0 (struct message, 1);
  gchar *unescaped_msg, *original_msg;

  /* 原始的消息会被释放，所以得到一份拷贝。 */
  unescaped_msg = g_strdup (purple_unescape_html (message));
  original_msg	= g_strdup (message);

  d->conv = conv;
  d->text = original_msg;
  d->pd	  = pd;

  NEW_WATCHER_W ();
  w->ok	  = u2u_message_ok;
  w->err  = u2u_message_err;
  w->data = d;

  g_queue_push_tail (pd->queue, w);
  axon_client_fsend_plain (pd->fd, purple_conversation_get_name (conv), unescaped_msg);
  g_free (unescaped_msg);
}

void
u2u_img_message_send (PurpleConnection * pc, PurpleConversation * conv,
		      int id)
{
  struct message	*d	  = g_new0 (struct message, 1);
  PurpleStoredImage	*image;
  gchar			*b64_dptr = NULL;
  const guchar		*raw_dptr;

  PD_FROM_PTR (pc->proto_data);

  image = purple_imgstore_find_by_id (id);
  if (image == NULL)
    return;
  
  raw_dptr = purple_imgstore_get_data (image);
  b64_dptr = purple_base64_encode (raw_dptr, purple_imgstore_get_size (image));

  d->text = g_strdup_printf ("\n<IMG ID=\"%d\">", id);
  d->conv = conv;
  d->pd	  = pd;

  NEW_WATCHER_W ();
  w->ok	  = u2u_message_ok;
  w->err  = u2u_message_err;
  w->data = d;

  g_queue_push_tail (pd->queue, w);
  axon_client_fsend_image (pd->fd, purple_conversation_get_name (conv), b64_dptr);
  g_free (b64_dptr);
}

void
fetch_chat_members_ok (PurpleConnection * pc, gpointer c, Data data)
{
  PurpleConversation	*conv  = c;
  Data			 members, admins, owner, tmp0, tmp1;
  GList			*names = NULL, *flags = NULL;
  gint			 member_count, admin_count;

  json_object_object_get_ex (data, "list", &members);
  json_object_object_get_ex (data, "admin", &admins);
  json_object_object_get_ex (data, "owner", &owner);
  
  member_count = json_object_array_length (members);
  admin_count  = json_object_array_length (admins);

  gchar *s_name;

  for (gint i = 0; i < member_count; i++)
    {
      tmp0   = json_object_array_get_idx (members, i);
      s_name = g_strdup(json_object_get_string (tmp0));
      names  = g_list_prepend (names, s_name);
      /* 检查是否为管理员 */
      for (gint ii = 0; ii < admin_count; ii++)
	{
	  tmp1 = json_object_array_get_idx (admins, ii);
	  if (purple_strequal (s_name, json_object_get_string (tmp1)))
	    {
	      flags = g_list_prepend (flags, GINT_TO_POINTER (PURPLE_CBFLAGS_OP));
	      goto flagsOk;
	    }
	}
      /* 检查是否为群主 */
      if (purple_strequal (s_name, json_object_get_string (owner)))
	{
	  flags = g_list_prepend (flags, GINT_TO_POINTER (PURPLE_CBFLAGS_FOUNDER));
	  goto flagsOk;
	}
      flags = g_list_prepend (flags, GINT_TO_POINTER (PURPLE_CBFLAGS_NONE));
    flagsOk: {}
    }

  purple_conv_chat_add_users (PURPLE_CONV_CHAT (conv), names, NULL, flags, FALSE);
  g_list_free (names);
  g_list_free (flags);
}

void
fetch_chat_members_err (PurpleConnection * pc, gpointer conv, Data _)
{
  purple_conv_chat_write (PURPLE_CONV_CHAT (conv),
			  "Axon",
			  "同步群聊用户失败，请检查 AXON 服务器！"
			  "（可能您同意的群聊邀请需要管理员批准）",
			  PURPLE_MESSAGE_ERROR,
			  g_get_real_time () / 1000 / 1000);
}

void
update_chat_members (PurpleConnection * pc, PurpleConversation * conv)
{
  DEBUG_LOG ("updating group member list");
  PD_FROM_PTR (pc->proto_data);

  int chat_id = 0;
  chat_id = purple_conv_chat_get_id (PURPLE_CONV_CHAT (conv));

  NEW_WATCHER_W ();
  w->ok = fetch_chat_members_ok;
  w->err = fetch_chat_members_err;
  w->data = conv;
  g_queue_push_tail (pd->queue, w);

  char *s_id = g_malloc0 (sizeof (char) * 12);
  sprintf (s_id, "%d", chat_id);
  axon_client_fetch_group_members (pd->fd, s_id);
  g_free (s_id);
}
