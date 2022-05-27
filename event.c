#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <json-c/json.h>
#include <purple.h>

#include "axon.h"
#include "blist.h"
#include "chat.h"
#include "common.h"
#include "connection.h"
#include "conversation.h"
#include "debug.h"
#include "event.h"
#include "eventloop.h"
#include "imgstore.h"
#include "json_object.h"
#include "server.h"
#include "util.h"

typedef struct {
	char *chat_name;
	char *chat_id;
	char *inviter;

	PurpleConnection *pc;
} ChatInvitaion;

void
chat_new_arrival_cb(PurpleConnection *pc, Data event)
{
	PurpleConversation *conv;
	Data id, name;

	json_object_object_get_ex(event, "id", &id);
	json_object_object_get_ex(event, "name", &name);

	conv = purple_find_chat(pc, json_object_get_int(id));
	if (conv == NULL)
		return;

	purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
	    json_object_get_string(name), NULL, PURPLE_CBFLAGS_NONE, TRUE);
	DEBUG_LOG("cb arriving");
}

void
chat_recall_cb(PurpleConnection *pc, Data event)
{
	PurpleConversation *conv;
	char notification[32];
	Data id, name;

	json_object_object_get_ex(event, "id", &id);
	json_object_object_get_ex(event, "name", &name);

	conv = purple_find_chat(pc, json_object_get_int(id));
	if (conv == NULL)
		return;

	snprintf(notification, 31, "%s 撤回了一条消息。",
	    json_object_get_string(name));

	purple_conv_chat_write(PURPLE_CONV_CHAT(conv),
	    json_object_get_string(name), notification,
	    PURPLE_MESSAGE_SYSTEM, g_get_real_time()/1000/1000);
}

void
chat_leave_cb(PurpleConnection *pc, Data event)
{
	PurpleConversation *conv;
	Data id, name, reason;

	json_object_object_get_ex(event, "id", &id);
	json_object_object_get_ex(event, "name", &name);
	json_object_object_get_ex(event, "reason", &reason);

	conv = purple_find_chat(pc, json_object_get_int(id));
	if (conv == NULL)
		return;

	purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv),
	    json_object_get_string(name), json_object_get_string(id));
	DEBUG_LOG("cb leaving");
}

void
chat_invite_approve(ChatInvitaion *invitation, int _)
{
	PD_FROM_PTR(invitation->pc->proto_data);

	g_queue_push_tail(pd->queue, watcher_nil());
	axon_client_call(pd->fd, "APPROVE");

	GHashTable *components = g_hash_table_new_full(g_str_hash,
		    g_str_equal, NULL, g_free);
	g_hash_table_insert(components, PRPL_CHAT_INFOID,
	    invitation->chat_id);
	g_hash_table_insert(components, PRPL_CHAT_INFONAME,
	    invitation->chat_name);

	serv_got_chat_invite(invitation->pc, invitation->chat_name,
	    invitation->inviter, NULL, components);

	g_free(invitation->inviter);
	g_free(invitation);
}

void
chat_invite_refuse(ChatInvitaion *invitation, int _)
{
	PD_FROM_PTR(invitation->pc->proto_data);

	g_queue_push_tail(pd->queue, watcher_nil());
	axon_client_call(pd->fd, "REFUSE");

	g_free(invitation->chat_id);
	g_free(invitation->chat_name);
	g_free(invitation->inviter);
	g_free(invitation);
}

void
chat_invite_cb(PurpleConnection *pc, Data event)
{
	Data chat_name, inviter, chat_id;
	char *s_chat_name, *s_inviter, *s_chat_id;
	ChatInvitaion *invitation = g_new0(ChatInvitaion, 1);

	json_object_object_get_ex(event, "name", &chat_name);
	json_object_object_get_ex(event, "id", &chat_id);
	json_object_object_get_ex(event, "sender", &inviter);

	CLONE_STR(s_chat_name, json_object_get_string(chat_name));
	CLONE_STR(s_inviter, json_object_get_string(inviter));
	CLONE_STR(s_chat_id, json_object_get_string(chat_id));

	invitation->chat_id   = s_chat_id;
	invitation->chat_name = s_chat_name;
	invitation->inviter   = s_inviter;
	invitation->pc        = pc;

	char info[101];
	snprintf(info, 100, "收到来自 %s 的群聊邀请，目标群聊：%s (%s)。",
	    s_inviter, s_chat_name, s_chat_id);

	purple_request_action(pc, "群聊邀请", info, NULL, 1, pc->account,
	    json_object_get_string(inviter), NULL, invitation, 2,
	    "同意", chat_invite_approve,
	    "拒绝", chat_invite_refuse);
}

void
u2u_attention_cb(PurpleConnection *pc, Data event)
{
	Data id;
	json_object_object_get_ex(event, "sender", &id);
	serv_got_attention(pc, json_object_get_string(id), 0);
}

void
u2u_event_cb(PurpleConnection *pc, Data event)
{
	Data id, text, timestamp;

	json_object_object_get_ex(event, "sender", &id);
	json_object_object_get_ex(event, "text", &text);
	json_object_object_get_ex(event, "time", &timestamp);

	serv_got_im(pc, json_object_get_string(id),
	    json_object_get_string(text),
	    PURPLE_MESSAGE_RECV,
	    json_object_get_int64(timestamp));
}

void
c2u_event_cb(PurpleConnection *pc, Data event)
{
	Data id, name, time, text, sender;
	PurpleConversation *conv;

	json_object_object_get_ex(event, "id",     &id);
	json_object_object_get_ex(event, "name",   &name);
	json_object_object_get_ex(event, "time",   &time);
	json_object_object_get_ex(event, "text",   &text);
	json_object_object_get_ex(event, "sender", &sender);

	conv = purple_find_chat(pc, json_object_get_int(id));

	if (conv == NULL) {
		char *s_name;
		CLONE_STR(s_name, json_object_get_string(name));
		conv = serv_got_joined_chat(pc,
		    json_object_get_int(id), s_name);
		update_chat_members(pc, conv);
	}

	serv_got_chat_in(pc,
	    json_object_get_int(id),
	    json_object_get_string(sender),
	    PURPLE_MESSAGE_RECV,
	    json_object_get_string(text),
	    json_object_get_int64(time));
}

void
u2u_img_download_cb(PurpleUtilFetchUrlData *_, gpointer dptr,
    const gchar *buffer, gsize len, const gchar *error_message)
{
	MEDIA_INFO *media_info = dptr;
	char *data = g_malloc0(len + 1);
	int img_id;

	if (len == 0) {
		DEBUG_LOG("download failed");
		DEBUG_LOG(error_message);
		return;
	}

	memcpy(data, buffer, len);
	img_id = purple_imgstore_add_with_id(data, len, NULL);
	serv_got_im(media_info->pc, media_info->sender,
	    g_strdup_printf("\n<IMG ID=\"%d\">", img_id),
	    PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES,
	    media_info->timestamp);

	g_free(media_info->sender);
	g_free(media_info);
}

void
c2u_img_download_cb(PurpleUtilFetchUrlData *_, gpointer dptr,
    const gchar *buffer, gsize len, const gchar *error_message)
{
	MEDIA_INFO *media_info = dptr;
	char *data = g_malloc0(len + 1);
	int img_id;

	if (len == 0) {
		DEBUG_LOG("download failed");
		DEBUG_LOG(error_message);
		return;
	}

	memcpy(data, buffer, len);
	img_id = purple_imgstore_add_with_id(data, len, NULL);
	serv_got_chat_in(media_info->pc,
	    media_info->id,
	    media_info->sender,
	    PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES,
	    g_strdup_printf("\n<IMG ID=\"%d\">", img_id),
	    media_info->timestamp);

	g_free(media_info->sender);
	g_free(media_info);
}

void
c2u_img_event_cb(PurpleConnection *pc, Data event)
{
	MEDIA_INFO *media_info = g_malloc0(sizeof(MEDIA_INFO));
	Data id, name, time, url, sender;
	PurpleConversation *conv;

	json_object_object_get_ex(event, "id",     &id);
	json_object_object_get_ex(event, "name",   &name);
	json_object_object_get_ex(event, "time",   &time);
	json_object_object_get_ex(event, "url",    &url);
	json_object_object_get_ex(event, "sender", &sender);

	conv = purple_find_chat(pc, json_object_get_int(id));

	if (conv == NULL) {
		char *s_name;
		CLONE_STR(s_name, json_object_get_string(name));
		conv = serv_got_joined_chat(pc,
		    json_object_get_int(id), s_name);
		update_chat_members(pc, conv);
	}

	CLONE_STR(media_info->sender, json_object_get_string(sender));
	media_info->timestamp = json_object_get_int64(time);
	media_info->id = json_object_get_int(id);
	media_info->pc = pc;

	purple_util_fetch_url_len(json_object_get_string(url),
	    TRUE, NULL, TRUE, MEDIA_MAX_LEN, c2u_img_download_cb, media_info);
}

void
u2u_img_event_cb(PurpleConnection *pc, Data event)
{
	MEDIA_INFO *media_info = g_malloc0(sizeof(MEDIA_INFO));
	Data entry;

	json_object_object_get_ex(event, "sender", &entry);
	CLONE_STR(media_info->sender, json_object_get_string(entry));

	json_object_object_get_ex(event, "time", &entry);
	media_info->timestamp = json_object_get_int64(entry);

	media_info->pc = pc;

	json_object_object_get_ex(event, "url", &entry);

	purple_util_fetch_url_len(json_object_get_string(entry),
	    TRUE, NULL, TRUE, MEDIA_MAX_LEN, u2u_img_download_cb, media_info);
}

void
event_cb(gpointer data, gint _, PurpleInputCondition __)
{
	PD_FROM_PTR(data);
	Data top, type, status;
	Watcher *w = NULL;
	size_t bytes_read;

	DEBUG_LOG("getting an event");

	bytes_read = read(pd->fd, pd->buf, BUFSIZE);
	if (bytes_read == 0) {
		DEBUG_LOG("network failure");
		purple_connection_error_reason(pd->acct->gc,
		    PURPLE_CONNECTION_ERROR_OTHER_ERROR,
		    "丢失了到 AXON 的连接");
		purple_input_remove(pd->acct->gc->inpa);
		return;
	}

	pd->buf[bytes_read] = '\0';
	// DEBUG_LOG(pd->buf);

	top = json_tokener_parse(pd->buf);
	json_object_object_get_ex(top, "status", &status);
	json_object_object_get_ex(top, "type", &type);

	switch(json_object_get_int(status)) {
	case(RET_STATUS_EVENT):  /* 事件上报 */
		json_object_object_get_ex(top, "type", &type);
		switch (json_object_get_int(type)) {
		case(E_FRIEND_MESSAGE):
			u2u_event_cb(pd->acct->gc, top);
			break;
		case(E_FRIEND_IMG_MESSAGE):
			u2u_img_event_cb(pd->acct->gc, top);
			break;
		case(E_GROUP_MESSAGE):
			c2u_event_cb(pd->acct->gc, top);
			break;
		case(E_GROUP_IMG_MESSAGE):
			c2u_img_event_cb(pd->acct->gc, top);
			break;
		case(E_FRIEND_ATTENTION):
			u2u_attention_cb(pd->acct->gc, top);
			break;
		case(E_GROUP_INVITE):
			chat_invite_cb(pd->acct->gc, top);
			break;
		case(E_GROUP_INCREASE):
			chat_new_arrival_cb(pd->acct->gc, top);
			break;
		case(E_GROUP_DECREASE):
			chat_leave_cb(pd->acct->gc, top);
			break;
		case(E_GROUP_RECALL):
			chat_recall_cb(pd->acct->gc, top);
			break;
		default:
			DEBUG_LOG("unexpected event type");
			break;
		}
		break;
	case(RET_STATUS_OK):    /* 命令响应 */
		w = g_queue_pop_head(pd->queue);
		if (w == NULL) {
			DEBUG_LOG("unexpected data");
			DEBUG_LOG(pd->buf);
			return;
		}
		w->ok(pd->acct->gc, w->data, top);
		break;
	default:                /* 错误处理 */
		w = g_queue_pop_head(pd->queue);
		if (w == NULL) {
			DEBUG_LOG("unexpected data");
			DEBUG_LOG(pd->buf);
			return;
		}
		w->err(pd->acct->gc, w->data, top);
	}

	data_free(top);
	g_free(w);
}

void
watcher_nil_ok(PurpleConnection *_, gpointer ___, Data __)
{
	DEBUG_LOG("successfully got a response");
}

void
watcher_nil_err(PurpleConnection *_, gpointer ___, Data __)
{
	DEBUG_LOG("got a failed response");
}

Watcher *
watcher_nil()
{
	NEW_WATCHER_W();

	w->data = NULL;
	w->err  = watcher_nil_err;
	w->ok   = watcher_nil_ok;

	return w;
}
