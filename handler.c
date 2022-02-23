#include <fcntl.h>
#include <string.h>
#include <purple.h>
#include <unistd.h>
#include <json-c/json.h>

#include "conv.h"
#include "oicq.h"
#include "dconn.h"
#include "handler.h"

void
handle_im_send(PurpleConnection *pc, PurpleConversation *conv, const char *text)
{
    readinfo:
    struct oicq_conn *oicq = pc->proto_data;
    char *uid = purple_conversation_get_data(conv, "uid");

    if (uid == NULL) {
        char *uname = (char*) malloc(32*sizeof(char));
        char *store_id = (char*) malloc(12*sizeof(char));
        struct json_object *json_root, *name;

        strcpy(store_id, purple_conversation_get_name(conv));
        purple_conversation_set_data(conv, "uid", store_id);
        /* 补全 IM 信息 */
        send_user_info_req(oicq->fd, store_id);
        read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

        json_root = json_tokener_parse(oicq->inbuf);
        json_object_object_get_ex(json_root, "name", &name);
        strncpy(uname, json_object_get_string(name), 32);

        purple_conversation_set_data(conv, "uname", uname);
        purple_conversation_set_name(conv, uname);
        goto readinfo;
    }

    send_friend_plain_message(oicq->fd, uid, text);
    purple_conv_im_write(PURPLE_CONV_IM(conv),
                           oicq->whoami,
                           text,
                           PURPLE_MESSAGE_SEND, g_get_real_time()/1000/1000);
}

void
handle_chat_send(PurpleConnection *pc, PurpleConversation *conv, const char *text)
{
    struct oicq_conn *oicq = pc->proto_data;
    char *id = purple_conversation_get_data(conv, "id");

    g_assert(id != NULL);

    send_group_plain_message(oicq->fd, id, text);
    purple_conv_chat_write(PURPLE_CONV_CHAT(conv),
                           oicq->whoami,
                           text,
                           PURPLE_MESSAGE_SEND, g_get_real_time()/1000/1000);
}

void
handle_friend_msg(PurpleConnection *pc, struct json_object *body)
{
    struct json_object *uid;
    struct json_object *text;
    struct json_object *timestamp;
    PurpleConversation *conv;
    char id[12];

    json_object_object_get_ex(body, "sender", &uid);
    json_object_object_get_ex(body, "text", &text);
    json_object_object_get_ex(body, "time", &timestamp);

    /* 获取字符串形式的 QQ 帐号 */
    snprintf(id, 12, "%ldd", json_object_get_int64(uid));

    conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY,
                                                 id, pc->account);
    if (conv == NULL)
        conv = create_friend_conv(pc, id);

    const char *uname = purple_conversation_get_data(conv, "uname");
    serv_got_im(pc, uname,
                json_object_get_string(text),
                PURPLE_MESSAGE_RECV, json_object_get_int64(timestamp));
}

void
handle_group_msg(PurpleConnection *pc, struct json_object *body)
{
    struct json_object *gid;
    struct json_object *text;
    struct json_object *timestamp;
    struct json_object *sender_name;
    PurpleConversation *conv;
    char id[12];

    json_object_object_get_ex(body, "id", &gid);
    json_object_object_get_ex(body, "text", &text);
    json_object_object_get_ex(body, "time", &timestamp);
    json_object_object_get_ex(body, "sender", &sender_name);

    snprintf(id, 12, "%ldg", json_object_get_int64(gid));

    conv = purple_find_chat(pc, g_str_hash(id));
    if (conv == NULL)
        conv = create_group_conv(pc, id);

    serv_got_chat_in(pc, g_str_hash(id),
                     json_object_get_string(sender_name), PURPLE_MESSAGE_RECV,
                     json_object_get_string(text), json_object_get_int64(timestamp));
}
