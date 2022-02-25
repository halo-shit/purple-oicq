#include <unistd.h>
#include <glib.h>

#include "conv.h"
#include "oicq.h"
#include "dconn.h"
#include "handler.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "connection.h"
#include "debug.h"
#include "prpl.h"
#include "notify.h"
#include "plugin.h"
#include "version.h"

int sockfd = -1;
PurplePlugin *oicq_plugin = NULL;

/**
 * 在 Socket 收到新消息时被调用，事件更新。
 */
static void
do_event_check_cb(gpointer data, gint source, PurpleInputCondition _)
{
    int t;
    struct json_object *type;
    struct json_object *status;
    struct json_object *json_root;

    PurpleConnection *pc = data;
    struct oicq_conn *oicq = pc->proto_data;

    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "status", &status);

    if (json_object_get_int(status) == RET_STATUS_EVENT) {
        json_object_object_get_ex(json_root, "type", &type);
        t = json_object_get_int(type);

        if (t == E_FRIEND_MESSAGE)
            handle_friend_msg(pc, json_root);
        else if (t == E_GROUP_MESSAGE)
            handle_group_msg(pc, json_root);
    }
}

static void
do_whoami_check_cb(gpointer data, gint source, PurpleInputCondition _)
{
    PurpleConnection *pc = data;
    struct json_object *status;
    struct json_object *json_root;
    struct oicq_conn *oicq = pc->proto_data;

    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "status", &status);

    if (json_object_get_int(status)) {
        purple_connection_error_reason(pc,
                                    PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                    "没能查询到身份");
        return;
    }

    json_object_object_get_ex(json_root, "name", &status);
    oicq->whoami = json_object_get_string(status);

    purple_input_remove(pc->inpa);
    purple_connection_set_state(pc, PURPLE_CONNECTED);
    /* 监听 Socket 连接，向 cb 传入 PurpleConnection */
    pc->inpa = purple_input_add(oicq->fd, PURPLE_INPUT_READ, do_event_check_cb, pc);
}

static void
do_login_check_cb(gpointer data, gint source, PurpleInputCondition _)
{
    PurpleConnection *pc = data;
    struct json_object *status;
    struct json_object *json_root;
    struct oicq_conn *oicq = pc->proto_data;

    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "status", &status);

    if (json_object_get_int(status)) {
        purple_connection_error_reason(pc,
                                    PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                    "没能登录到 QQ 服务");
        return;
    }
    purple_input_remove(pc->inpa);
    /* 步骤四：我是谁？ */
    purple_connection_update_progress(pc, "查询身份", 2, 4);
    send_command(sockfd, "WHOAMI");
    pc->inpa = purple_input_add(oicq->fd, PURPLE_INPUT_READ, do_whoami_check_cb, pc);
}

static void
do_init_check_cb(gpointer data, gint source, PurpleInputCondition _)
{
    PurpleConnection *pc = data;
    struct json_object *status;
    struct json_object *json_root;
    struct oicq_conn *oicq = pc->proto_data;

    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "status", &status);

    if (json_object_get_int(status)) {
        purple_connection_error_reason(pc,
                                    PURPLE_CONNECTION_ERROR_OTHER_ERROR,
                                    "没能初始化 OICQ 客户端");
        return;
    }

    purple_input_remove(pc->inpa);
    /* 步骤三：登录到 QQ 服务 */
    purple_connection_update_progress(pc, "登录到 QQ", 2, 4);
    oicq_login(&sockfd, pc->account->password);
    pc->inpa = purple_input_add(oicq->fd, PURPLE_INPUT_READ, do_login_check_cb, pc);
}

/**
 * 用于获得指定好友/帐号的图标，目前只返回单独一种。
 *
 * 如果好友是 NULL 而帐号是非 NULL，返回对应的文字来显示帐号图标。
 * 如果都为 NULL，则返回协议的图标。
 */
static const char *
prpl_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
    return "oicq";
}

/**
 * 用于获取一份装有对用户可用的状态的列表。
 */
static GList *
prpl_status_types(PurpleAccount *acct)
{
    GList *types = NULL;
    PurpleStatusType *type;
    /* 加入离线状态 */
    type = purple_status_type_new(PURPLE_STATUS_OFFLINE, "offline", "离线",
            TRUE);
    types = g_list_prepend(types, type);
    /* 加入在线状态 */
    type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, "online", "我在线上",
            TRUE);
    types = g_list_prepend(types, type);
    type = purple_status_type_new(PURPLE_STATUS_AWAY, "busy", "忙碌",
            TRUE);
    types = g_list_prepend(types, type);

    return types;
}

/**
 * 用于获取一系列消息，来将聊天添加到好友列表。
 *
 * 其中的第一项是一个独一无二的名字，用于在执行 purple_blist_find_chat 时，
 * 确认好友列表的聊天。(在这里是与 QQ 群号关联的文本)
 */
static GList *
prpl_chat_info(PurpleConnection *gc)
{
    /* 定义在 prpl.h */
    struct proto_chat_entry *pce;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = "群号:"; /* 需要本地化 */
    pce->identifier = PRPL_CHAT_INFO_QQ_GID;
    pce->required = TRUE;

    return g_list_append(NULL, pce);
}

/**
 * 用于获取默认聊天信息。
 */
static GHashTable *
prpl_chat_info_defaults(PurpleConnection *gc,
                                          const char *chat)
{
    GHashTable *defaults;

    defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
    return defaults;
}

/**
 * 建立到 OICQ 守护程序和 QQ 帐号的连接。
 */
void
prpl_login(PurpleAccount *acct)
{
    char buf[GENERAL_BUF_SIZE];
    struct oicq_conn *oicq;
    PurpleConnection *pc = purple_account_get_connection(acct);

    /* 步骤一：连接到 Axon */
    purple_debug_info(PRPL_ID, "connecting to Axon\n");
    purple_connection_set_state(pc, PURPLE_CONNECTING);
    purple_connection_update_progress(pc, "连接到后端", 0, 3);

    if (oicq_connect(&sockfd,
                     purple_account_get_string(acct, PRPL_ACCOUNT_OPT_HOST, "127.0.0.1"),
                     purple_account_get_string(acct, PRPL_ACCOUNT_OPT_PORT, "9999")) < 0) {
        purple_connection_error_reason(pc,
                                    PURPLE_CONNECTION_ERROR_OTHER_ERROR,
                                    "没能连接到 OICQ 服务器");
        return;
    }
    /* 将 OCIQ 信息装入 Purple */
    purple_debug_info(PRPL_ID, "loading data\n");
    g_assert(purple_connection_get_protocol_data(pc) == NULL);
    oicq = g_new0(struct oicq_conn, 1);
    oicq->account = acct;
    oicq->fd = sockfd;
    oicq->inbuf = buf;
    purple_connection_set_protocol_data(pc, oicq);

    /* 步骤二：初始化 OICQ 客户端 */
    purple_debug_info(PRPL_ID, "initializing\n");
    purple_connection_update_progress(pc, "初始化客户端", 1, 3);
    oicq_init(&sockfd, acct->username);
    pc->inpa = purple_input_add(oicq->fd, PURPLE_INPUT_READ, do_init_check_cb, pc);
}

/**
 * 断开到 OICQ 守护程序和 QQ 帐号的连接。
 */
static void
prpl_close(PurpleConnection *pc)
{
    /* 断开 Socket，清理内存 */
}

/**
 * 用于获取聊天的名字。
 */
static char *
prpl_get_chat_name(GHashTable *components)
{
    const char *room = g_hash_table_lookup(components, PRPL_CHAT_INFO_QQ_GID);
    return g_strdup(room);
}


/**
 * 处理在好友列表中双击聊天，或接收一个聊天邀请。
 */
static void
prpl_join_chat(PurpleConnection *pc, GHashTable *components)
{
    char chat_id[12];
    const char *gid;
    PurpleConvChat *chat;
    PurpleConversation *conv;

    gid = g_hash_table_lookup(components, PRPL_CHAT_INFO_QQ_GID);
    g_assert(gid != NULL);

    snprintf(chat_id, 12, "%sg", gid);

    conv = purple_find_chat(pc, g_str_hash(chat_id));

    if(!conv) {
        conv = create_group_conv(pc, chat_id);
        return;
    }

    chat = PURPLE_CONV_CHAT(conv);
    chat->left = FALSE;

    if (!g_slist_find(pc->buddy_chats, conv))
            pc->buddy_chats = g_slist_append(pc->buddy_chats, conv);
    purple_conversation_update(conv, PURPLE_CONV_UPDATE_CHATLEFT);
}


/**
 * 处理拒绝聊天。
 */
static void
prpl_reject_chat(PurpleConnection *gc, GHashTable *components)
{
    /* 什么也不做 */
}

/**
 * 处理邀请聊天。
 */
static void
prpl_chat_invite(PurpleConnection *gc, int id,
        const char *message, const char *who)
{
    /* Invite */
}

/**
 * 处理退出聊天：告诉服务器，并清理内存。
 */
static void
prpl_chat_leave(PurpleConnection *gc, int id) {
    PurpleConversation *conv = purple_find_chat(gc, id);
    purple_debug_info(PRPL_ID, "%s is leaving chat room %s\n",
                      gc->account->username, conv->name);
}


/**
 * 处理发送群聊消息。
 */
static int
prpl_chat_send(PurpleConnection *pc, int id,
        const char *message, PurpleMessageFlags flags) {
    PurpleConversation *conv = purple_find_chat(pc, id);
    if(!conv) {
        return -1;
    }

    handle_chat_send(pc, conv, message);
    /* 发送消息 */
    return 0;
}

/**
 * 处理发送私聊消息。
 */
static int
prpl_im_send(PurpleConnection *pc, const char *who,
        const char *message, PurpleMessageFlags flags) {
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, pc->account);
    if(!conv) {
        return -1;
    }

    handle_im_send(pc, conv, message);
    /* 发送消息 */
    return 0;
}


/**
 * 获取用户的用户 ID，给予他们在房间中的显示名称。
 *
 * @returns 字符串，会被调用者清理。
 */
static char *
prpl_get_cb_real_name(PurpleConnection *gc, int id,
        const char *who)
{
    PurpleConversation *conv = purple_find_chat(gc, id);
    struct json_object *json_root, *uid;
    struct oicq_conn *oicq = gc->proto_data;
    char *ret = malloc(12*sizeof(char));

    if(conv == NULL)
        return NULL;
    send_lookup_nickname(oicq->fd, who);

    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);

    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "uid", &uid);
    strcpy(ret, json_object_get_string(uid));

    return ret;
}

/**
 * 动作列表，在插件菜单中显示。
 */
static GList *
prpl_actions(PurplePlugin *plugin, gpointer context)
{
  GList *list = NULL;

  return list;
}

static GList *
prpl_attention_types(PurpleAccount *acct)
{
    /* 初始化窗口抖动 */
    PurpleAttentionType *pat;
    pat = purple_attention_type_new("shake", "shake",
                                    "您收到了一个窗口抖动！",
                                    "您发送了一个窗口抖动！");
    return g_list_append(NULL, pat);
}

static gboolean
prpl_send_attention(PurpleConnection *pc, const char *who, guint type)
{
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, pc->account);

    readinfo:
    struct oicq_conn *oicq = pc->proto_data;
    char *uid = purple_conversation_get_data(conv, "uid");

    if (uid == NULL) {
        init_im_conv(pc, conv);
        goto readinfo;
    }

    send_friend_poke(oicq->fd, uid);
    return TRUE;
}

static void
prpl_get_info(PurpleConnection *pc, const char *who)
{
    struct oicq_conn *oicq = pc->proto_data;
    struct json_object *json_root, *uid, *sex;
    PurpleNotifyUserInfo *user_info;

    /* 获取对应的 QQ 号 */
    send_lookup_nickname(oicq->fd, who);
    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);
    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "uid", &uid);

    /* 获取对应用户的信息 */
    send_info_req_by_id(oicq->fd, json_object_get_string(uid));
    read(oicq->fd, oicq->inbuf, GENERAL_BUF_SIZE);
    json_root = json_tokener_parse(oicq->inbuf);
    json_object_object_get_ex(json_root, "sex", &sex);

    user_info = purple_notify_user_info_new();
    purple_notify_user_info_add_pair(user_info, "替代昵称", who);
    purple_notify_user_info_add_pair(user_info, "QQ号", json_object_get_string(uid));
    purple_notify_user_info_add_pair(user_info, "性别", json_object_get_string(sex));

    purple_notify_userinfo(pc, who, user_info, NULL, NULL);
}

static void
prpl_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
{
    purple_prpl_got_user_status(pc->account, buddy->name, "online", NULL);

    // purple_account_request_add(pc->account, buddy->name, NULL, NULL, NULL);
}


/* 协议描述 */
static PurplePluginProtocolInfo prpl_info =
{
    OPT_PROTO_UNIQUE_CHATNAME | /* 独一无二的用户名 */
      OPT_PROTO_CHAT_TOPIC |    /* 具有聊天主题（群/好友 描述/签名） */
      OPT_PROTO_IM_IMAGE,       /* 聊天支持图像（图片/表情） */
    NULL,                       /* 用户名分割, 稍后在 prpl_init() 中初始化 */
    NULL,                       /* 协议选项，稍后在 prpl_init() 中初始化 */
    {   /* PurpleBuddyIconSpec (好友头像) */
        "png,jpg,gif",                   /* 格式 */
        0,                               /* 最小宽度 */
        0,                               /* 最小高度 */
        128,                             /* 最大宽度 */
        128,                             /* 最大高度 */
        10000,                           /* 最大文件大小 */
        PURPLE_ICON_SCALE_DISPLAY,       /* 伸缩规则 */
    },
    prpl_list_icon,                        /* 列表图标 */
    NULL,                                  /* 列表象征 */
    NULL,                                  /* 状态信息 */
    NULL,                                  /* 提示信息 */
    prpl_status_types,                     /* 状态类型 */
    NULL,                                  /* 好友列表节点的右键菜单 */
    prpl_chat_info,                        /* 聊天信息 */
    prpl_chat_info_defaults,               /* 默认聊天信息 */
    prpl_login,                            /* 登录 */
    prpl_close,                            /* 关闭 */
    prpl_im_send,                          /* 发送 IM */
    NULL,                                  /* 设置信息 */
    NULL,                                  /* 发送正在输入 */
    prpl_get_info,                         /* 获取信息 */
    NULL,                                  /* 设置状态 */
    NULL,                                  /* 设置Idle */
    NULL,                                  /* 修改密码 */
    prpl_add_buddy,                        /* 加好友 */
    NULL,                                  /* 加好友 */
    NULL,                                  /* 删好友 */
    NULL,                                  /* 删好友 */
    NULL,                                  /* 设置允许 */
    NULL,                                  /* 设置阻止 */
    NULL,                                  /* 重设允许 */
    NULL,                                  /* 重设阻止 */
    NULL,                                  /* 设置允许/阻止 */
    prpl_join_chat,                        /* 加入聊天 */
    prpl_reject_chat,                      /* 拒绝聊天 */
    prpl_get_chat_name,                    /* 获取聊天名称 */
    prpl_chat_invite,                      /* 聊天邀请 */
    prpl_chat_leave,                       /* 退出聊天 */
    NULL,                                  /* ？？？ */
    prpl_chat_send,                        /* 发送聊天 */
    NULL,                                  /* keepalive */
    NULL,                                  /* 注册用户 */
    NULL,                                  /* 已废弃 */
    NULL,                                  /* 已废弃 */
    NULL,                                  /* 好友别名 */
    NULL,                                  /* 好友分组 */
    NULL,                                  /* 重命名组 */
    NULL,                                  /* 删除好友 */
    NULL,                                  /* 关闭的群聊 */
    NULL,                                  /* 普通化 */
    NULL,                                  /* 设置好友图标 */
    NULL,                                  /* 删除群组 */
    prpl_get_cb_real_name,                 /* 获取聊天参与者的真名 */
    NULL,                                  /* 设置聊天话题 */
    NULL,                                  /* 查找好友列表聊天 */
    NULL,                                  /* 获取房间列表 */
    NULL,                                  /* 取消房间列表 */
    NULL,                                  /* 房间列表扩展分类 */
    NULL,                                  /* 接收文件 */
    NULL,                                  /* 发送文件 */
    NULL,                                  /* 新 Xfer */
    NULL,                                  /* 离线消息 */
    NULL,                                  /* ？？？ */
    NULL,                                  /* RAW 发送 */
    NULL,                                  /* 房间列表房间序列化 */
    NULL,                                  /* 注销用户 */
    prpl_send_attention,                   /* 发送Attention */
    prpl_attention_types,                  /* 获取Attention类别 */
    sizeof(PurplePluginProtocolInfo),      /* 结构体大小 */
    NULL,                                  /* 获取用户文本表 */
    NULL,                                  /* initiate_media */
    NULL,                                  /* get_media_caps */
    NULL,                                  /* 获取Moods */
    NULL,                                  /* 设置公共别名 */
    NULL,                                  /* 获取公共别名 */
    NULL,                                  /* 加好友，附带邀请 */
    NULL                                   /* 加好友，附带邀请 */
};

static void
prpl_init(PurplePlugin *plugin)
{
    /* 初始化协议选项 */
    struct _PurpleKeyValuePair *pair0, *pair1, *pair2;
    GList *login_options = NULL;
    GList *protocol_options = NULL;

    pair0 = g_new0(struct _PurpleKeyValuePair, 1);
    pair1 = g_new0(struct _PurpleKeyValuePair, 1);
    pair2 = g_new0(struct _PurpleKeyValuePair, 1);

    pair0->key = "密码登录";
    pair0->value = PRPL_ACCOUNT_OPT_USE_PASSWORD;
    pair1->key = "扫码登录";
    pair1->value = PRPL_ACCOUNT_OPT_USE_QRCODE;
    pair2->key = "扫码登录（单次）";
    pair2->value = PRPL_ACCOUNT_OPT_USE_QRCODE_ONCE;

    login_options = g_list_append(login_options, pair0);
    login_options = g_list_append(login_options, pair1);
    login_options = g_list_append(login_options, pair2);

    protocol_options = g_list_append(protocol_options,
            purple_account_option_list_new("登录方式", PRPL_ACCOUNT_OPT_LOGIN,
                    login_options));
    protocol_options = g_list_append(protocol_options,
            purple_account_option_string_new(
                    "服务器主机名", PRPL_ACCOUNT_OPT_HOST,
                    "127.0.0.1"));
    protocol_options = g_list_append(protocol_options,
            purple_account_option_string_new(
                    "服务器端口号", PRPL_ACCOUNT_OPT_PORT,
                    "9999"));

    prpl_info.protocol_options = protocol_options;
}

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
    "A binding to takayama-lily's OICQ library. Require a OICQ daemon to function correctly.",
    "axon-oicq@riseup.net",
    PRPL_WEBSITE,

    NULL,
    NULL,
    NULL,

    NULL,
    &prpl_info,
    NULL,
    prpl_actions,
    NULL,
    NULL,
    NULL,
    NULL
};


PURPLE_INIT_PLUGIN(oicq, prpl_init, info)
