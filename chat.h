#ifndef CHAT_H_GUARD
#define CHAT_H_GUARD

#include <purple.h>

void u2u_message_send(PurpleConnection*, PurpleConversation*, const char *);

void update_chat_members(PurpleConnection *, PurpleConversation *);

void u2c_message_send(PurpleConnection *, PurpleConversation *, const char *);

#endif
