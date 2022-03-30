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
#include "json_object.h"
#include "server.h"
#include "chat.h"

struct message {
	PurpleConversation *conv;
	ProtoData *pd;
	char *text;
};

void
u2c_message_ok(PurpleConnection *pc, gpointer data, Data _)
{
	struct message *d = data;

	DEBUG_LOG("group message ok");

	purple_conv_chat_write(PURPLE_CONV_CHAT(d->conv),
	    d->pd->whoami, d->text,
	    PURPLE_MESSAGE_SEND, g_get_real_time()/1000/1000);

	g_free(d->text);
	g_free(d);
}

void
u2c_message_err(PurpleConnection *pc, gpointer data, Data _)
{
	struct message *d = data;

	purple_conv_chat_write(PURPLE_CONV_CHAT(d->conv),
	    "Axon", "发送失败，请检查连接。",
	    PURPLE_MESSAGE_ERROR, g_get_real_time()/1000/1000);

	g_free(d->text);
	g_free(d);
}

void
u2c_message_send(PurpleConnection *pc, PurpleConversation *conv,
    const char *message)
{
	NEW_WATCHER_W();
	PD_FROM_PTR(pc->proto_data);
	struct message *d = g_new0(struct message, 1);
	char *msg;

	/* 原始的消息会被释放，所以得到一份拷贝。 */
	CLONE_STR(msg, message);

	d->conv = conv;
	d->text = msg;
	d->pd   = pd;

	w->ok   = u2c_message_ok;
	w->err  = u2c_message_err;
	w->data = d;
	g_queue_push_tail(pd->queue, w);

	char *s_id = g_malloc0(sizeof(char)*12);
	sprintf(s_id, "%d", purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)));
	axon_client_gsend_plain(pd->fd, s_id, message);
}

void
u2u_message_ok(PurpleConnection *pc, gpointer data, Data _)
{
	struct message *d = data;

	purple_conv_im_write(PURPLE_CONV_IM(d->conv),
	    d->pd->whoami,
	    d->text,
	    PURPLE_MESSAGE_SEND, g_get_real_time()/1000/1000);

	g_free(d->text);
	g_free(d);
}

void
u2u_message_err(PurpleConnection *pc, gpointer data, Data _)
{
	struct message *d = data;

	purple_conv_im_write(PURPLE_CONV_IM(d->conv),
	    "Axon", "发送失败，请检查连接。",
	    PURPLE_MESSAGE_ERROR, g_get_real_time()/1000/1000);

	g_free(d->text);
	g_free(d);
}

void
u2u_message_send(PurpleConnection *pc, PurpleConversation *conv,
    const char* message)
{
	NEW_WATCHER_W();
	PD_FROM_PTR(pc->proto_data);
	struct message *d = g_new0(struct message, 1);
	char *msg;

	/* 原始的消息会被释放，所以得到一份拷贝。 */
	CLONE_STR(msg, message);

	d->conv = conv;
	d->text = msg;
	d->pd   = pd;

	w->ok   = u2u_message_ok;
	w->err  = u2u_message_err;
	w->data = d;

	g_queue_push_tail(pd->queue, w);
	axon_client_fsend_plain(pd->fd,
	    purple_conversation_get_name(conv), message);
}

void
fetch_chat_members_ok(PurpleConnection *pc, gpointer c, Data data)
{
	PurpleConversation *conv = c;
	Data members, admins, owner, tmp0, tmp1;
	GList *names = NULL, *flags = NULL;
	size_t member_count, admin_count;

	json_object_object_get_ex(data, "list", &members);
	json_object_object_get_ex(data, "admin", &admins);
	json_object_object_get_ex(data, "owner", &owner);
	member_count = json_object_array_length(members);
	admin_count = json_object_array_length(admins);

	char *s_name;

	for (size_t i = 0; i < member_count; i++) {
		tmp0 = json_object_array_get_idx(members, i);
		CLONE_STR(s_name, json_object_get_string(tmp0));

		names = g_list_prepend(names, s_name);
		/* 检查是否为管理员 */
		for (size_t ii = 0; ii < admin_count; ii++) {
			tmp1 = json_object_array_get_idx(admins, ii);
			if (STR_IS_EQUAL(s_name,json_object_get_string(tmp1))) {
				flags = g_list_prepend(flags,
				    GINT_TO_POINTER(PURPLE_CBFLAGS_OP));
				goto flagsOk;
			}
		}
		/* 检查是否为群主 */
		if (STR_IS_EQUAL(s_name, json_object_get_string(owner))) {
			flags = g_list_prepend(flags,
			    GINT_TO_POINTER(PURPLE_CBFLAGS_FOUNDER));
			goto flagsOk;
		}
		flags = g_list_prepend(flags,
		    GINT_TO_POINTER(PURPLE_CBFLAGS_NONE));
 flagsOk: {}
	}

	purple_conv_chat_add_users(PURPLE_CONV_CHAT(conv),
	    names, NULL, flags, FALSE);
	g_list_free(names);
	g_list_free(flags);
}

void
fetch_chat_members_err(PurpleConnection *pc, gpointer conv, Data _)
{
	purple_conv_chat_write(PURPLE_CONV_CHAT(conv),
	    "Axon", "同步群聊用户失败，请检查 AXON 服务器！"
	    "（可能您同意的群聊邀请需要管理员批准）",
	    PURPLE_MESSAGE_ERROR, g_get_real_time()/1000/1000);
}

void
update_chat_members(PurpleConnection *pc, PurpleConversation *conv)
{
	DEBUG_LOG("updating group member list");
	PD_FROM_PTR(pc->proto_data);

	int chat_id = 0;
	chat_id = purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv));

	NEW_WATCHER_W();
	w->ok   = fetch_chat_members_ok;
	w->err  = fetch_chat_members_err;
	w->data = conv;
	g_queue_push_tail(pd->queue, w);

	char *s_id = g_malloc0(sizeof(char)*12);
	sprintf(s_id, "%d", chat_id);
	axon_client_fetch_group_members(pd->fd, s_id);
	g_free(s_id);
}