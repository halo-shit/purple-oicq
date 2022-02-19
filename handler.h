#include <glib.h>
#include <purple.h>

#ifndef HANDLER_H_INCLUDED
#define HANDLER_H_INCLUDED

#define RET_STATUS_EVENT 1

/* 事件类型 */
#define E_FRIEND_MESSAGE 1
#define E_GROUP_MESSAGE 2

// void do_event_check_cb(gpointer, gint, const gchar*);

void handle_im_send(PurpleConnection*, PurpleConversation*, const char*);

void handle_chat_send(PurpleConnection*, PurpleConversation*, const char*);

void handle_friend_msg(PurpleConnection*, struct json_object*);

void handle_group_msg(PurpleConnection*, struct json_object*);

#endif // HANDLER_H_INCLUDED
