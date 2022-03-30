#ifndef EVENT_H
#define EVENT_H

#include <purple.h>

#include "common.h"
#include "conversation.h"

typedef void (*CallbackFunction)(PurpleConnection*, gpointer,  Data);

typedef struct {
	/* 回调函数 */
	CallbackFunction ok;
	CallbackFunction err;
	/* 自定义内容  */
	gpointer data;
} Watcher;

void event_cb(gpointer, gint, PurpleInputCondition);

Watcher *watcher_nil();

#define RET_STATUS_OK 0
#define RET_STATUS_EVENT 1

/* 事件类型 */
#define E_FRIEND_MESSAGE   1
#define E_GROUP_MESSAGE    2
#define E_FRIEND_ATTENTION 3
#define E_GROUP_INVITE     4
#define E_GROUP_INCREASE   5
#define E_GROUP_DECREASE   6

#endif