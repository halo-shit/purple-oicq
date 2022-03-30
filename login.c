#include "login.h"
#include "axon.h"
#include "blist.h"
#include "common.h"
#include "connection.h"
#include "conversation.h"
#include "event.h"
#include "json_object.h"
#include <string.h>

#include <glib.h>

void
purple_login_err(PurpleConnection *pc, gpointer message, Data _)
{
	purple_connection_error_reason(pc,
	    PURPLE_CONNECTION_ERROR_OTHER_ERROR,
	    message);
}

void
axon_client_glist_ok(PurpleConnection *pc, gpointer _, Data data)
{
	DEBUG_LOG("sync groups");

	int chat_count;
	GHashTable *components;
	char *s_name; char *s_id;
	Data name, name_list, id, id_list;
	PurpleGroup *group; PurpleChat *chat;

	json_object_object_get_ex(data, "idlist", &id_list);
	json_object_object_get_ex(data, "namelist", &name_list);
	chat_count = json_object_array_length(id_list);
	/* 如果对应分组不存在就新建一个 */
	group = purple_find_group(PRPL_SYNC_GROUP_CHAT);
	if (group == NULL) {
		DEBUG_LOG("group not found, will create one");
		group = purple_group_new(PRPL_SYNC_GROUP_CHAT);
		purple_blist_add_group(group, NULL);
	} else {
		goto skip;
	}
	/* 将群聊加入分组 */
	for (int i = 0; i < chat_count; i++) {
		name = json_object_array_get_idx(name_list, i);
		id   = json_object_array_get_idx(id_list, i);

		CLONE_STR(s_id, json_object_get_string(id));
		CLONE_STR(s_name, json_object_get_string(name));

		components = g_hash_table_new_full(g_str_hash,
		    g_str_equal, NULL, g_free);
		g_hash_table_insert(components, PRPL_CHAT_INFOID, s_id);
		g_hash_table_insert(components, PRPL_CHAT_INFONAME, s_name);
		chat = purple_chat_new(pc->account, s_name, components);

		purple_blist_add_chat(chat, group, NULL);
	}

 skip: {}
	purple_connection_set_state(pc, PURPLE_CONNECTED);
}

void
axon_client_flist_ok(PurpleConnection *pc, gpointer _, Data data)
{
	DEBUG_LOG("sync friends");

	PD_FROM_PTR(pc->proto_data);
	int friend_count; const char *s_name;
	PurpleGroup *group; PurpleBuddy *buddy;
	Data name, list;

	json_object_object_get_ex(data, "list", &list);
	friend_count = json_object_array_length(list);
	/* 如果对应分组不存在就新建一个 */
	group = purple_find_group(PRPL_SYNC_GROUP_BUDDY);
	if (group == NULL) {
		group = purple_group_new(PRPL_SYNC_GROUP_BUDDY);
		purple_blist_add_group(group, NULL);
	}
	/* 将好友加入分组 */
	for (int i = 0; i < friend_count; i++) {
		name = json_object_array_get_idx(list, i);
		s_name = json_object_get_string(name);
		/* 若用户已存在，则直接设置在线状态 */
 set_online_status: {}
		if (purple_find_buddy(pc->account, s_name) != NULL) {
			purple_prpl_got_user_status(pc->account,
			    s_name, "online", NULL);
			continue;
		}
		/* 不显示 BabyQ */
		if (!strcmp(s_name, "babyQ"))
			continue;
		buddy = purple_buddy_new(pc->account, s_name, s_name);
		purple_blist_add_buddy(buddy, NULL, group, NULL);
		goto set_online_status;
	}

	NEW_WATCHER_W();
	w->ok   = axon_client_glist_ok;
	w->err  = purple_login_err;
	w->data = "没能同步群聊列表";
	g_queue_push_tail(pd->queue, w);

	axon_client_call(pd->fd, "GLIST");
}

void
axon_client_whoami_ok(PurpleConnection *pc, gpointer _, Data response)
{
	PD_FROM_PTR(pc->proto_data);
	Data name;

	DEBUG_LOG("whoami ok, login completed");
	json_object_object_get_ex(response, "name", &name);

	char *whoami = g_malloc0(sizeof(char)*
	    (json_object_get_string_len(name) + 1));
	strcpy(whoami, json_object_get_string(name));

	pd->whoami = whoami;

	NEW_WATCHER_W();
	w->data = "没能同步好友列表";
	w->ok   = axon_client_flist_ok;
	w->err  = purple_login_err;
	g_queue_push_tail(pd->queue, w);

	axon_client_call(pd->fd, "FLIST");
}

void
axon_client_login_ok(PurpleConnection *pc, gpointer _, Data __)
{
	PD_FROM_PTR(pc->proto_data);

	DEBUG_LOG("login ok, wating for next procedure");
	purple_connection_update_progress(pc, "查询身份", 2, 5);

	NEW_WATCHER_W();
	w->data = "没能查询到身份";
	w->ok   = axon_client_whoami_ok;
	w->err  = purple_login_err;
	g_queue_push_tail(pd->queue, w);

	axon_client_call(pd->fd, "WHOAMI");
}

void
axon_client_init_ok(PurpleConnection *pc, gpointer _, Data __)
{
	PD_FROM_PTR(pc->proto_data);

	DEBUG_LOG("init ok, wating for next procedure");
	purple_connection_update_progress(pc, "登录到 QQ", 2, 5);

	NEW_WATCHER_W();
	w->data = "没能登录到 QQ 服务";
	w->ok   = axon_client_login_ok;
	w->err  = purple_login_err;
	g_queue_push_tail(pd->queue, w);

	axon_client_login(pd->fd, pc->account->password);
}