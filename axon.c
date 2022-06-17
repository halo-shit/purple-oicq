#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "axon.h"
#include <json-glib/json-glib.h>
#include "common.h"

typedef JsonObject* Data;
JsonGenerator *g = NULL;
JsonNode *n = NULL;

Data
data_new ()
{
  if (g == NULL)
    g = json_generator_new ();
  if (n == NULL)
    n = json_node_new (JSON_NODE_OBJECT);
  return json_object_new ();
}

void
data_free (Data data)
{
  json_object_unref (data);
}

void
data_set_param (Data data, const char *k, const char *v)
{
  json_object_set_string_member (data, k, v);
}

void
data_set_cmd (Data data, const char *cmd)
{
  data_set_param (data, "command", cmd);
}

int
data_write_to_fd (Data data, int fd)
{
  if (fd == -1)
    return -1;

  json_node_set_object (n, data);
  json_generator_set_root (g, n);
  
  const char	*plain;
  gsize		 length;
  
  plain = json_generator_to_data (g, &length);
  write (fd, plain, length);
  return write (fd, "\n", 1);
}

int
axon_connect (const char *host, const char *port)
{
  int			sockfd;
  struct sockaddr_in	conn_param;
  
  sockfd		     = socket (AF_INET, SOCK_STREAM, 0);
  conn_param.sin_addr.s_addr = inet_addr (host);
  conn_param.sin_port	     = htons (atoi (port));
  conn_param.sin_family	     = AF_INET;

  int conn_ret;
  conn_ret = connect (sockfd, (struct sockaddr *) &conn_param,
		      sizeof (conn_param));
  if (conn_ret == -1)
    {
      conn_ret = errno;
      perror ("Error in axon_client_connect");
      return -1;
    }

  return sockfd;
}

void
axon_client_init (int sockfd, const char *self_id, const char *platform)
{
  Data j = data_new ();
  data_set_param (j, "uin", self_id);
  data_set_param (j, "platform", platform);
  data_set_cmd (j, "INIT");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_password_login (int sockfd, const char *passwd)
{
  Data j = data_new ();
  data_set_param (j, "method", "0");
  data_set_param (j, "passwd", passwd);
  data_set_cmd (j, "LOGIN");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_qrcode_login (int sockfd)
{
  Data j = data_new ();
  data_set_param (j, "method", "1");
  data_set_cmd (j, "LOGIN");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_call (int sockfd, const char *cmd)
{
  Data j = data_new ();
  data_set_cmd (j, cmd);
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_fsend_shake (int sockfd, const char *id)
{
  Data j = data_new ();
  data_set_param (j, "id", id);
  data_set_cmd (j, "USEND_SHAKE");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_fsend_plain (int sockfd, const char *id, const char *message)
{
  Data j = data_new ();
  data_set_param (j, "message", message);
  data_set_param (j, "id", id);
  data_set_cmd (j, "USEND");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_fsend_image (int sockfd, const char *id, const char *data)
{
  Data j = data_new ();
  data_set_param (j, "data", data);
  data_set_param (j, "id", id);
  data_set_cmd (j, "USEND_IMG");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_gsend_plain (int sockfd, const char *id, const char *message)
{
  Data j = data_new ();
  data_set_param (j, "message", message);
  data_set_param (j, "id", id);
  data_set_cmd (j, "GSEND");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_gsend_image (int sockfd, const char *id, const char *data)
{
  Data j = data_new ();
  data_set_param (j, "data", data);
  data_set_param (j, "id", id);
  data_set_cmd (j, "GSEND_IMG");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_fetch_acct_info (int sockfd, const char *id)
{
  Data j = data_new ();
  data_set_param (j, "id", id);
  data_set_cmd (j, "IDINFO");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_fetch_group_info (int sockfd, const char *id)
{
  Data j = data_new ();
  data_set_param (j, "id", id);
  data_set_cmd (j, "GINFO");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_fetch_group_members (int sockfd, const char *id)
{
  Data j = data_new ();
  data_set_param (j, "id", id);
  data_set_cmd (j, "GMLIST");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_lookup_nickname (int sockfd, const char *nick)
{
  Data j = data_new ();
  data_set_param (j, "nickname", nick);
  data_set_cmd (j, "LOOKUP");
  data_write_to_fd (j, sockfd);
  data_free (j);
}

void
axon_client_update_status (int sockfd, const char *new_status)
{
  Data j = data_new ();
  data_set_param (j, "status", new_status);
  data_set_cmd (j, "STATUS");
  data_write_to_fd (j, sockfd);
  data_free (j);
}
