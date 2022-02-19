#ifndef CONV_H_INCLUDED
#define CONV_H_INCLUDED

PurpleConversation* create_friend_conv(PurpleConnection*, char*);

PurpleConversation* create_group_conv(PurpleConnection*, char*);

void init_group_conv_ulist(PurpleConnection*, PurpleConversation*, char*);

#endif // CONV_H_INCLUDED
