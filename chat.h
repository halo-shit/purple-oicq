#ifndef CHAT_H_GUARD
#define CHAT_H_GUARD

#include <purple.h>
#include <glib.h>
#include "common.h"

void u2u_message_send (PurpleConnection *, PurpleConversation *, const gchar *);

void u2u_img_message_send (PurpleConnection *, PurpleConversation *, gint);

void update_chat_members (PurpleConnection *, PurpleConversation *);

void u2c_message_send (PurpleConnection *, PurpleConversation *, const gchar *);

void u2c_img_message_send (PurpleConnection *, PurpleConversation *, gint);

void lookup_ok (PurpleConnection *, gpointer, JsonReader *);

void lookup_err (PurpleConnection *, gpointer, JsonReader *);

#endif
