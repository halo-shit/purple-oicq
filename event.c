#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <purple.h>
#include <json-glib/json-glib.h>

#include "axon.h"
#include "chat.h"
#include "common.h"
#include "connection.h"
#include "event.h"
#include "glibconfig.h"

typedef struct
{
  gchar			*chat_name;
  gchar			*chat_id;
  gchar			*inviter;
  PurpleConnection	*pc;
} ChatInvitaion;

void
chat_new_arrival_cb (PurpleConnection *pc, JsonReader *event)
{
  PurpleConversation	*conv;
  const gchar		*name;
  gint64		 id;

  json_reader_read_int (event, "id", id);
  json_reader_read_string (event, "name", name);
  
  conv = purple_find_chat (pc, id);
  
  if (conv == NULL)
    return;

  purple_conv_chat_add_user (PURPLE_CONV_CHAT (conv), name, NULL,
			     PURPLE_CBFLAGS_NONE, TRUE);
  DEBUG_LOG ("cb arriving");
}

void
chat_recall_cb (PurpleConnection *pc, JsonReader *event)
{
  PurpleConversation	*conv;
  const gchar		*name;
  gchar			 notification[32];
  gint64		 id;

  json_reader_read_int (event, "id", id);
  json_reader_read_string (event, "name", name);
  
  conv = purple_find_chat (pc, id);
  
  if (conv == NULL)
    return;

  g_snprintf (notification, 31, "%s 撤回了一条消息。", name);

  purple_conv_chat_write (PURPLE_CONV_CHAT (conv),
			  "Axon", notification,
			  PURPLE_MESSAGE_SYSTEM,
			  g_get_real_time () / 1000 / 1000);
}

void
chat_leave_cb (PurpleConnection *pc, JsonReader *event)
{
  PurpleConversation	*conv;
  const gchar	        *name;
  gint64		 id;

  json_reader_read_int (event, "id", id);
  json_reader_read_string (event, "name", name);

  conv = purple_find_chat (pc, id);
  
  if (conv == NULL)
    return;
  
  purple_conv_chat_remove_user (PURPLE_CONV_CHAT (conv), name, "kicked");
  DEBUG_LOG ("cb leaving");
}

void
chat_invite_approve (ChatInvitaion *invitation, int _)
{
  PD_FROM_PTR (invitation->pc->proto_data);

  g_queue_push_tail (pd->queue, watcher_nil ());
  axon_client_call (pd->fd, "APPROVE");

  GHashTable *components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
  g_hash_table_insert (components, PRPL_CHAT_INFOID, invitation->chat_id);
  g_hash_table_insert (components, PRPL_CHAT_INFONAME, invitation->chat_name);

  serv_got_chat_invite (invitation->pc, invitation->chat_name,
			invitation->inviter, NULL, components);

  g_free (invitation->inviter);
  g_free (invitation);
}

void
chat_invite_refuse (ChatInvitaion *invitation, int _)
{
  PD_FROM_PTR (invitation->pc->proto_data);

  g_queue_push_tail (pd->queue, watcher_nil ());
  axon_client_call (pd->fd, "REFUSE");

  g_free (invitation->chat_id);
  g_free (invitation->chat_name);
  g_free (invitation->inviter);
  g_free (invitation);
}

void
chat_invite_cb (PurpleConnection *pc, JsonReader *event)
{
  gchar		 info[100];
  const gchar	*chat_name, *inviter, *chat_id;
  ChatInvitaion *invitation = g_new0 (ChatInvitaion, 1);

  json_reader_read_string (event, "name", chat_name);
  json_reader_read_string (event, "id", chat_id);
  json_reader_read_string (event, "sender", inviter);

  invitation->chat_id	= g_strdup (chat_id);
  invitation->chat_name = g_strdup (chat_name);
  invitation->inviter	= g_strdup (inviter);
  invitation->pc	= pc;

  g_snprintf (info, 99, "收到来自 %s 的群聊邀请，目标群聊：%s (%s)。",
	    inviter, chat_name, chat_id);

  purple_request_action (pc, "群聊邀请", info, NULL, 1, pc->account,
			 inviter, NULL, invitation, 2,
			 "同意", chat_invite_approve,
			 "拒绝", chat_invite_refuse);
}

void
u2u_attention_cb (PurpleConnection *pc, JsonReader *event)
{
  const gchar	*sender;
  json_reader_read_string (event, "sender", sender);
  serv_got_attention (pc, sender, 0);
}

void
u2u_event_cb (PurpleConnection *pc, JsonReader *event)
{
  const gchar   *text, *sender;
  gint64	 time;

  json_reader_read_string (event, "sender", sender);
  json_reader_read_string (event, "text", text);
  
  json_reader_read_int (event, "time", time);

  serv_got_im (pc, sender, text, PURPLE_MESSAGE_RECV, time);
}

void
c2u_event_cb (PurpleConnection *pc, JsonReader *event)
{
  const gchar	        *name, *text, *sender;
  gint64		 time, id;
  PurpleConversation	*conv;

  json_reader_read_string (event, "name", name);
  json_reader_read_string (event, "text", text);
  json_reader_read_string (event, "sender", sender);

  json_reader_read_int (event, "id", id);
  json_reader_read_int (event, "time", time);

  conv = purple_find_chat (pc, id);

  if (conv == NULL)
    {
      gchar *s_name = g_strdup (name);
      conv = serv_got_joined_chat (pc, id, s_name);
      update_chat_members (pc, conv);
    }

  serv_got_chat_in (pc, id, sender, PURPLE_MESSAGE_RECV, text, time);
}

void
u2u_img_download_cb (PurpleUtilFetchUrlData *_, gpointer dptr,
		     const gchar *buffer, gsize len,
		     const gchar *error_message)
{
  guchar	*data	    = g_malloc0 (len + 1);
  MEDIA_INFO	*media_info = dptr;
  gint		 img_id;

  if (len == 0)
    {
      DEBUG_LOG ("download failed");
      DEBUG_LOG (error_message);
      return;
    }

  memcpy (data, buffer, len);
  img_id = purple_imgstore_add_with_id (data, len, NULL);
  serv_got_im (media_info->pc, media_info->sender,
	       g_strdup_printf ("\n<IMG ID=\"%d\">", img_id),
	       PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES,
	       media_info->timestamp);

  g_free (media_info->sender);
  g_free (media_info);
}

void
c2u_img_download_cb (PurpleUtilFetchUrlData *_, gpointer dptr,
		     const gchar *buffer, gsize len,
		     const gchar *error_message)
{
  MEDIA_INFO *media_info = dptr;
  guchar *data = g_malloc0 (len + 1);
  gint img_id;

  if (len == 0)
    {
      DEBUG_LOG ("download failed");
      DEBUG_LOG (error_message);
      return;
    }

  memcpy (data, buffer, len);
  img_id = purple_imgstore_add_with_id (data, len, NULL);
  serv_got_chat_in (media_info->pc, media_info->id, media_info->sender,
		    PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES,
		    g_strdup_printf ("\n<IMG ID=\"%d\">", img_id),
		    media_info->timestamp);

  g_free (media_info->sender);
  g_free (media_info);
}

void
c2u_img_event_cb (PurpleConnection *pc, JsonReader *event)
{
  MEDIA_INFO		*media_info = g_malloc0 (sizeof (MEDIA_INFO));
  const gchar		*name, *url, *sender;
  gint64		 time, id;
  PurpleConversation	*conv;

  json_reader_read_string (event, "name", name);
  json_reader_read_string (event, "url", url);
  json_reader_read_string (event, "sender", sender);

  json_reader_read_int (event, "id", id);
  json_reader_read_int (event, "time", time);

  conv = purple_find_chat (pc, id);

  if (conv == NULL)
    {
      conv = serv_got_joined_chat (pc, id, name);
      update_chat_members (pc, conv);
    }

  media_info->sender	= g_strdup (sender);
  media_info->timestamp = time;
  media_info->id	= id;
  media_info->pc	= pc;

  purple_util_fetch_url_len (url, TRUE, NULL, TRUE, MEDIA_MAX_LEN,
			     c2u_img_download_cb, media_info);
}

void
u2u_img_event_cb (PurpleConnection *pc, JsonReader *event)
{
  MEDIA_INFO *media_info = g_malloc0 (sizeof (MEDIA_INFO));
  const char *entry;

  json_reader_read_string (event, "sender", entry);
  media_info->sender = g_strdup (entry);

  json_reader_read_string (event, "time", entry);
  media_info->timestamp = atoi (entry);

  media_info->pc = pc;

  json_reader_read_string (event, "url", entry);

  purple_util_fetch_url_len (entry, TRUE, NULL, TRUE, MEDIA_MAX_LEN,
			     u2u_img_download_cb, media_info);
}

void
event_cb (gpointer data, gint _, PurpleInputCondition __)
{
  PD_FROM_PTR (data);
  NEW_WATCHER_W ();
  gint		 retval, status, type;
  GError	*error	    = NULL;
  gsize		 bytes_read = 0;

  DEBUG_LOG ("getting an event");

  /* 按数字前缀的长度读取 Json 数据流 */
  
  while (TRUE)
    {
      if (bytes_read + 1024 > pd->buf_size)
	{
	  pd->buf = g_realloc (pd->buf, pd->buf_size + 1024);
	  pd->buf_size += 1024;
	  continue;
	}

      bytes_read += read (pd->fd, pd->buf + bytes_read, 1024);
      if (bytes_read == 0)
	    {
	      DEBUG_LOG ("network failure");
	      purple_connection_error_reason (pd->acct->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					      "丢失了到 AXON 的连接");
	      purple_input_remove (pd->acct->gc->inpa);
	      return;
	    }
      
      
      if (pd->buf[bytes_read - 1] == '\n')
	{
	  pd->buf[bytes_read - 1] = 0;
	  break;
	}
    }

  
  // DEBUG_LOG(pd->buf);

  retval = json_parser_load_from_data (pd->parser, pd->buf, bytes_read - 1, &error);
  if (retval == FALSE)
    {
      DEBUG_LOG ("failed to parse json data");
      purple_connection_error_reason (pd->acct->gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
				      "没能处理 Json 数据");
      purple_input_remove (pd->acct->gc->inpa);
    }

  json_reader_set_root (pd->reader, json_parser_get_root (pd->parser));

  json_reader_read_int (pd->reader, "status", status);
  
  switch (status)
    {
    case (RET_STATUS_EVENT):	/* 事件上报 */
      json_reader_read_int (pd->reader, "type", type);
      switch (type)
	{
	case (E_FRIEND_MESSAGE):
	  u2u_event_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_FRIEND_IMG_MESSAGE):
	  u2u_img_event_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_GROUP_MESSAGE):
	  c2u_event_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_GROUP_IMG_MESSAGE):
	  c2u_img_event_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_FRIEND_ATTENTION):
	  u2u_attention_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_GROUP_INVITE):
	  chat_invite_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_GROUP_INCREASE):
	  chat_new_arrival_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_GROUP_DECREASE):
	  chat_leave_cb (pd->acct->gc, pd->reader);
	  break;
	case (E_GROUP_RECALL):
	  chat_recall_cb (pd->acct->gc, pd->reader);
	  break;
	default:
	  DEBUG_LOG ("unexpected event type");
	  break;
	}
      break;
    case (RET_STATUS_OK):	/* 命令响应 */
      w = g_queue_pop_head (pd->queue);
      if (w == NULL)
	{
	  DEBUG_LOG ("unexpected data");
	  DEBUG_LOG (pd->buf);
	  return;
	}
      w->ok (pd->acct->gc, w->data, pd->reader);
      break;
    default:			/* 错误处理 */
      w = g_queue_pop_head (pd->queue);
      if (w == NULL)
	{
	  DEBUG_LOG ("unexpected data");
	  DEBUG_LOG (pd->buf);
	  return;
	}
      w->err (pd->acct->gc, w->data, pd->reader);
    }

  g_free (w);
}

void
watcher_nil_ok (PurpleConnection *_, gpointer ___, JsonReader *__)
{
  DEBUG_LOG ("successfully got a response");
}

void
watcher_nil_err (PurpleConnection *_, gpointer ___, JsonReader *__)
{
  DEBUG_LOG ("got a failed response");
}

Watcher *
watcher_nil ()
{
  NEW_WATCHER_W ();

  w->data = NULL;
  w->err  = watcher_nil_err;
  w->ok	  = watcher_nil_ok;

  return w;
}
