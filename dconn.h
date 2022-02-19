#include <json-c/json.h>

#ifndef CONN_H_INCLUDED
#define CONN_H_INCLUDED

int oicq_connect(int*);

void send_command(int, char*);

void send_friend_message(int, char*, struct json_object*);

void send_friend_plain_message(int, char*, const char*);

void send_group_message(int, char*, struct json_object*);

void send_group_plain_message(int, char*, const char*);

void send_user_info_req(int, char*);

void send_group_info_req(int, char*);

void send_group_members_req(int, char*);

int oicq_init(int*, char*);

int oicq_login(int*, char*);

int oicq_state(int*);

#define GENERAL_BUF_SIZE 20480

#endif // CONN_H_INCLUDED
