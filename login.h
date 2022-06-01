#ifndef LOGIN_H
#define LOGIN_H

#include <purple.h>

#include "common.h"

typedef struct
{
  PurpleConnection	*pc;
  gchar			*name;
} BUDDY_INFO;

enum
{
  L_DEVICE = 0,
  L_ERROR  = 1,
  L_SLIDER = 2,
  L_QRCODE = 3,
};

void axon_client_init_ok (PurpleConnection *, gpointer, Data);

void purple_init_err (PurpleConnection *, gpointer, Data);

#endif
