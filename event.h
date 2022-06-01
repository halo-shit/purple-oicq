#ifndef EVENT_H
#define EVENT_H

#include <purple.h>

#include "common.h"
#include "connection.h"
#include "conversation.h"

typedef void (*CallbackFunction) (PurpleConnection *, gpointer, Data);

typedef struct
{
  PurpleConversation	*conv;
  PurpleConnection	*pc;
  time_t		 timestamp;
  gchar			*sender;
  gchar			*url;
  gint			 id;
} MEDIA_INFO;

typedef struct
{
  /* 回调函数 */
  CallbackFunction	ok;
  CallbackFunction	err;
  /* 自定义内容  */
  gpointer		data;
} Watcher;

void event_cb (gpointer, gint, PurpleInputCondition);

Watcher *watcher_nil ();

enum
{
  RET_STATUS_OK = 0,
  RET_STATUS_EVENT = 1,
};

/* 事件类型 */
enum
{
  E_FRIEND_MESSAGE = 1,
  E_GROUP_MESSAGE = 2,
  E_FRIEND_ATTENTION = 3,
  E_GROUP_INVITE = 4,
  E_GROUP_INCREASE = 5,
  E_GROUP_DECREASE = 6,
  E_GROUP_IMG_MESSAGE = 7,
  E_FRIEND_IMG_MESSAGE = 8,
  E_GROUP_RECALL = 9,
};

#define MEDIA_MAX_LEN 1024*1024*10

#endif
