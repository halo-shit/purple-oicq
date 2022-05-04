#ifndef CHAT_H_GUARD
#define CHAT_H_GUARD

#include <purple.h>
#include "common.h"

void u2u_message_send(PurpleConnection*, PurpleConversation*, const char *);

void update_chat_members(PurpleConnection *, PurpleConversation *);

void u2c_message_send(PurpleConnection *, PurpleConversation *, const char *);

void lookup_ok(PurpleConnection *, gpointer, Data);

void lookup_err(PurpleConnection *, gpointer, Data);

#endif
