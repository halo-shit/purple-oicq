#ifndef LOGIN_H
#define LOGIN_H

#include <purple.h>

#include "common.h"

void axon_client_init_ok(PurpleConnection*, gpointer, Data);

void purple_login_err(PurpleConnection*, gpointer, Data);

#endif
