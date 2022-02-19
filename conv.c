#include <json-c/json.h>
#include <purple.h>
#include <string.h>
#include <unistd.h>

#include "dconn.h"
#include "oicq.h"

/**
 * 初始化群聊的用户列表，Pidgin 不会提示。
 *
 * @param pc PurpleConnection
 * @param conv PurpleConversation
 * @param id 不带标识符的 QQ 群号
 */
void init_group_conv_ulist(PurpleConnection *pc, PurpleConversation *conv, char *id)
{
    struct oicq_conn *oicq = pc->proto_data;
    struct json_object *json_root;
    struct json_object *members;
    struct json_object *tmp_obj;
    size_t n_members;
    GList *names = NULL, *flags = NULL;

    send_group_members_req(oicq->fd, id);
    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "list", &members);
    n_members = json_object_array_length(members);

    for (int i=0; i<n_members; i++)
    {
        tmp_obj = json_object_array_get_idx(members, i);
        names = g_list_prepend(names, (gpointer)json_object_get_string(tmp_obj));
        flags = g_list_prepend(flags, GINT_TO_POINTER(0));
    }

    purple_conv_chat_add_users(PURPLE_CONV_CHAT(conv), names, NULL, flags, FALSE);
    g_list_free(names);
    g_list_free(flags);
}

/**
 * 新建一个好友聊天，并绑定到对应 PurpleConnection 的用户上。
 *
 * @param pc PurpleConnection
 * @param fixed_uid 后面带有标识符的 QQ 号（例：0000000000d）
 */
PurpleConversation* create_friend_conv(PurpleConnection *pc, char* fixed_uid)
{
    struct oicq_conn *oicq = pc->proto_data;
    struct json_object *json_root;
    struct json_object *name;
    PurpleConversation *conv;

    /* 去除末尾的标识符，为获取信息做准备 */
    char *uid = (char*) malloc(12*sizeof(char));
    char *uname = (char*) malloc(32*sizeof(char));
    strncpy(uid, fixed_uid, 12);
    uid[strlen(fixed_uid)-1] = '\0';

    // conv = serv_got_joined_chat(pc, g_str_hash(fixed_uid), fixed_uid);

    send_user_info_req(oicq->fd, uid);

    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);
    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "name", &name);
    strncpy(uname, json_object_get_string(name), 32);

    conv = purple_conversation_new(PURPLE_CONV_TYPE_IM,
                                   pc->account,
                                   uname);
    /* 将 QQ 号存入 Conversation */
    purple_conversation_set_data(conv, "uid", uid);
    purple_conversation_set_data(conv, "uname", uname);
    return conv;
}

/**
 * 新建一个群组聊天，并绑定到对应 PurpleConnection 的用户上。
 *
 * @param pc PurpleConnection
 * @param fixed_id 后面带有标识符的 QQ 群号（例：000000000g）
 */
PurpleConversation* create_group_conv(PurpleConnection *pc, char* fixed_id)
{
    struct oicq_conn *oicq = pc->proto_data;
    struct json_object *json_root;
    struct json_object *name;
    PurpleConversation *conv;

    /* 去除末尾的标识符，为获取信息做准备 */
    char *id = (char*) malloc(12*sizeof(char));
    strncpy(id, fixed_id, 11);
    id[strlen(fixed_id)-1] = '\0';
    purple_debug_info(PRPL_ID, id);
    /* 发送信息请求 */
    send_group_info_req(oicq->fd, id);
    /* 读取信息 */
    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);
    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "name", &name);

    /* 新建 Chat 并设置 ID */
    conv = serv_got_joined_chat(pc, g_str_hash(id), json_object_get_string(name));
    purple_conv_chat_set_id(PURPLE_CONV_CHAT(conv), g_str_hash(fixed_id));
    init_group_conv_ulist(pc, conv, id);
    /* 将群号存入 Conversation */
    purple_conversation_set_data(conv, "id", id);
    return conv;
}
