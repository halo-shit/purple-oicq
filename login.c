#include "login.h"
#include "account.h"
#include "axon.h"
#include "blist.h"
#include "common.h"
#include "connection.h"
#include "conversation.h"
#include "event.h"
#include "json_object.h"
#include "notify.h"
#include "request.h"
#include <stdio.h>
#include <string.h>

#include <glib.h>

void axon_client_login_err(PurpleConnection *, gpointer, Data);

void
purple_init_err(PurpleConnection *pc, gpointer message, Data _)
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
 set_online_status:
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
	w->err  = purple_init_err;
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
	w->err  = purple_init_err;
	g_queue_push_tail(pd->queue, w);

	axon_client_call(pd->fd, "FLIST");
}

void
axon_client_login_ok(PurpleConnection *pc, gpointer _, Data __)
{
	/* 将单次扫码登录改为密码登录 */
	if (STR_IS_EQUAL(purple_account_get_string(pc->account,
	    PRPL_ACCT_OPT_LOGIN, PRPL_ACCT_OPT_USE_PASSWORD),
	    PRPL_ACCT_OPT_USE_QRCODE_ONCE))
		purple_account_set_string(pc->account, PRPL_ACCT_OPT_LOGIN,
		    PRPL_ACCT_OPT_USE_PASSWORD);

	PD_FROM_PTR(pc->proto_data);

	DEBUG_LOG("login ok, wating for next procedure");
	purple_connection_update_progress(pc, "查询身份", 2, 5);

	NEW_WATCHER_W();
	w->data = "没能查询到身份";
	w->ok   = axon_client_whoami_ok;
	w->err  = purple_init_err;
	g_queue_push_tail(pd->queue, w);

	axon_client_call(pd->fd, "WHOAMI");
}

void
go_ahead_cb(void *dptr)
{
	PurpleConnection *pc = dptr;
	PD_FROM_PTR(pc->proto_data);

	NEW_WATCHER_W();
	w->ok   = axon_client_login_ok;
	w->err  = axon_client_login_err;
	g_queue_push_tail(pd->queue, w);

	axon_client_call(pd->fd, "GOAHEAD");
}

void
ticket_submit_cb()
{

}

void
axon_client_login_err(PurpleConnection *pc, gpointer _, Data data)
{
	PD_FROM_PTR(pc->proto_data);
	Data login_t, tmp;

	json_object_object_get_ex(data, "login", &login_t);

	switch (json_object_get_int(login_t)) {
	case L_ERROR:
		json_object_object_get_ex(data, "message", &tmp);
		purple_connection_error_reason(pc,
		    PURPLE_CONNECTION_ERROR_OTHER_ERROR,
		    json_object_get_string(tmp));
		break;
	case L_SLIDER:
		json_object_object_get_ex(data, "url", &tmp);
		sprintf(pd->buf, "本次登录需要滑块登录，验证码：%s。"
		    "请在滑块完成后输入 ticket。",
		    json_object_get_string(tmp));

		purple_request_input(pc, "登录验证", pd->buf, NULL, "", FALSE,
		    FALSE, NULL, "提交", NULL, "取消", NULL,
		    pd->acct, NULL, NULL, NULL);
		break;
	case L_QRCODE:
		/* 保存图片并显示 */
		break;
	case L_DEVICE:
		json_object_object_get_ex(data, "url", &tmp);
		purple_notify_message(pc, PURPLE_NOTIFY_MSG_WARNING, "登录验证",
		    "本次登录需要解锁设备锁，请前往弹出链接解锁，并在完成后关闭此窗口提交。",
		    NULL, go_ahead_cb, pc);
		purple_notify_uri(pc, json_object_get_string(tmp));
		break;
	}
}

void
test_cb()
{}

void
axon_client_init_ok(PurpleConnection *pc, gpointer _, Data __)
{
	PD_FROM_PTR(pc->proto_data);

	DEBUG_LOG("init ok, wating for next procedure");
	purple_connection_update_progress(pc, "登录到 QQ", 2, 5);

	NEW_WATCHER_W();
	w->ok   = axon_client_login_ok;
	w->err  = axon_client_login_err;
	g_queue_push_tail(pd->queue, w);

	/* 登录方式检查 */
	if (STR_IS_EQUAL(purple_account_get_string(pc->account,
	    PRPL_ACCT_OPT_LOGIN, PRPL_ACCT_OPT_USE_PASSWORD),
	    PRPL_ACCT_OPT_USE_PASSWORD)) {
		axon_client_password_login(pd->fd, pc->account->password);
	} else {
		axon_client_qrcode_login(pd->fd);
	}
}
