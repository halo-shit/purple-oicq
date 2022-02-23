#include <json-c/json.h>

#ifndef CONN_H_INCLUDED
#define CONN_H_INCLUDED

int oicq_connect(int*, const char*, const char*);

void send_command(int, const char*);

void send_friend_message(int, const char*, struct json_object*);

void send_friend_plain_message(int, const char*, const char*);

void send_group_message(int, const char*, struct json_object*);

void send_group_plain_message(int, const char*, const char*);

void send_user_info_req(int, const char*);

void send_group_info_req(int, const char*);

void send_group_members_req(int, const char*);

void send_group_member_lookup(int, const char*, const char*);

void send_friend_lookup(int, const char*);

int oicq_init(int*, const char*);

int oicq_login(int*, const char*);

int oicq_state(int*);

#define GENERAL_BUF_SIZE 40960

#endif // CONN_H_INCLUDED
