#include "network.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <getopt.h>
#include <netinet/in.h>
// #include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

static struct option server_long_options[] = {
    {"port", required_argument, 0, 'p'},
    {"password", required_argument, 0, 'w'},
    {"charset-file", required_argument, 0, 'c'},
    {"timeout-ms", required_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

static struct option client_long_options[] = {
    {"host", required_argument, 0, 'o'},
    {"port", required_argument, 0, 'p'},
    {"id", required_argument, 0, 'i'},
    {"heartbeat-ms", required_argument, 0, 't'},
    {"threads", required_argument, 0, 'e'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

int parse_client_args(int argc, char *argv[], char **o, char **p, char **i,
                      char **t, char **e) {
  int opt;
  while ((opt = getopt_long(argc, argv, "o:p:i:t:e:h", client_long_options,
                            NULL)) != -1) {

    switch (opt) {
    case 'o':
      *o = optarg;
      break;

    case 'p':
      *p = optarg;
      break;

    case 'i':
      *i = optarg;
      break;

    case 't':
      *t = optarg;
      break;

    case 'e':
      *e = optarg;
      break;

    case 'h':
      printf("Usage: ./controller --host <host> --port <port> "
             "--id <worker-id> --heartbeat-ms <ms>\n");
      return 1;

    default:
      return -1;
    }
  }

  return 0;
}

int parse_server_args(int argc, char *argv[], char **p, char **w, char **c,
                      char **t) {
  int opt;
  while ((opt = getopt_long(argc, argv, "p:w:c:t:h", server_long_options,
                            NULL)) != -1) {

    switch (opt) {
    case 'p':
      *p = optarg;
      break;

    case 'w':
      *w = optarg;
      break;

    case 'c':
      *c = optarg;
      break;

    case 't':
      *t = optarg;
      break;

    case 'h':
      printf("Usage: ./controller --port <port> --password <password> "
             "--charset-file <path> --timeout-ms <ms>\n");
      return 1;

    default:
      return -1;
    }
  }

  return 0;
}

int create_socket(server_properties *s) {
  if (s->protocol == AF_INET) {
    s->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (s->socket == -1) {
      return -1;
    }
  }

  else {
    s->socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (s->socket == -1) {
      return -1;
    }
  }

  return 0;
};

int setup_server_address(server_properties *s, in_port_t *port) {

  if (s->protocol == AF_INET) {
    s->addr.sin_port = htons(*port);
    s->addr_len = sizeof(struct sockaddr_in);
    s->addr.sin_addr.s_addr = INADDR_ANY;
  }

  else {
    s->addr6.sin6_port = htons(*port);
    s->addr_len = sizeof(struct sockaddr_in6);
  }

  return 0;
};

int bind_socket(server_properties *s) {
  int status = 0;

  if (s->protocol == AF_INET) {
    if (bind(s->socket, (struct sockaddr *)&s->addr, s->addr_len) == -1) {
      perror("Binding Failed");
      return -1;
    }
  }

  else {
    if (bind(s->socket, (struct sockaddr *)&s->addr6, s->addr_len) == -1) {
      perror("Binding Failed");
      return -1;
    }
  }

  return 0;
};

int start_listen(server_properties *s) {
  if (listen(s->socket, 1) == -1) {
    return -1;
  }

  return 0;
};

int connect_client(server_properties *s) {
  if (s->protocol == AF_INET) {
    int status = connect(s->socket, (struct sockaddr *)&s->addr, s->addr_len);
    return status;
  }

  else {
    int status = connect(s->socket, (struct sockaddr *)&s->addr6, s->addr_len);
    return status;
  }
}

int valid_ip(const char *ip, server_properties *s) {
  struct in_addr ipv4;
  struct in6_addr ipv6;

  if (inet_pton(AF_INET, ip, &ipv4) == 1) {
    s->addr.sin_family = AF_INET;
    s->addr.sin_addr = ipv4;
    s->protocol = AF_INET;
    return 0;
  }

  if (inet_pton(AF_INET6, ip, &ipv6) == 1) {
    s->addr6.sin6_family = AF_INET6;
    s->addr6.sin6_addr = ipv6;
    s->protocol = AF_INET6;
    return 0;
  }

  return -1;
}

int parse_port(char *portstr, in_port_t *p) {

  // check for null
  if (portstr == NULL) {
    printf("ERROR: Port is null\n");
    return -1;
  }

  char *endptr;
  long longport = strtol(portstr, &endptr, 10);

  // invalid chars
  if (endptr == portstr || *endptr != '\0') {
    printf("ERROR: Invalid port\n");
    return -1;
  }

  // range
  if (longport < 1 || longport > 65535) {
    printf("ERROR: Port out of range\n");
    return -1;
  }

  *p = (in_port_t)longport;
  return 0;
}

int numcheck(char *nums) {
  for (int i = 0; i < strlen(nums); i++) {
    if (!isdigit(nums[i])) {
      return -1;
    }
  }

  return 0;
}

int validate_server_args(struct ServerArgs args, in_port_t *p) {

  // validate port
  if (parse_port(args.port_str, p) == -1) {
    return -1;
  }

  // charset_file
  if (access(args.charset_file, F_OK) == 1) {
    printf("ERROR: Charset File path does not exist\n");
    return -1;
  }

  // timeout
  if (numcheck(args.timeout_ms) == -1) {
    printf("ERROR: Invalid timeout value");
    return -1;
  }

  return 0;
}

int validate_client_args(struct ClientArgs args, in_port_t *p,
                         server_properties *s) {
  // host
  if (valid_ip(args.host, s) == -1) {
    printf("ERROR: Invalid IP address\n");
    return -1;
  }
  // port
  if (parse_port(args.port_str, p) == -1) {
    return -1;
  }

  // id
  if (numcheck(args.worker) == -1) {
    printf("ERROR: Invalid worker-id value");
    return -1;
  }

  // heartbeat
  if (numcheck(args.heartbeat) == -1) {
    printf("ERROR: Invalid heartbeat value");
    return -1;
  }

  return 0;
}


// -------------------------------------- MESSAGES
void send_ASSIGN(int clientfd, task_t *t) {
  char message[4096];

  t->state = ASSIGNED;

  snprintf(message, sizeof(message),
           "ASSIGN task=%d worker=%d start=%d end=%d length=%d hash=%s\n",
           t->task_id, t->assigned_worker, t->start, t->end, t->length,
           t->hash_value);

  size_t n = send(clientfd, message, strlen(message), 0);
  printf("sent: %d bytes\n", (int)n);
}

void send_COMPLETE(int clientfd, task_t *t) {
  char message[4096];

  t->state = COMPLETE;

  snprintf(message, sizeof(message),
           "COMPLETE task=%d worker=%d\n",
           t->task_id, t->assigned_worker);

  size_t n = send(clientfd, message, strlen(message), 0);
  printf("COMPLETE sent: %d bytes\n", (int)n);
}

void send_START(int serverfd, task_t *t) {
  char message[4096];

  snprintf(message, sizeof(message), "START task=%d worker=%d\n", t->task_id, t->assigned_worker);

  size_t n = send(serverfd, message, strlen(message), 0);
  printf("START sent: %d bytes\n", (int)n);
}

void send_HEARTBEAT(int serverfd, task_t *t, int progress) {
  char message[4096];

  snprintf(message, sizeof(message), "HEARTBEAT task=%d worker=%d state=%d progress=%d\n", t->task_id, t->assigned_worker, t->state, progress);

  size_t n = send(serverfd, message, strlen(message), 0);
  printf("HEARTBEAT sent: %d bytes\n", (int)n);
}

void send_RESULT(int serverfd, task_t *t, int index, char *candidate) {
  char message[4096];

  snprintf(message, sizeof(message), "RESULT task=%d worker=%d result=%d index=%d candidate=%s\n", t->task_id, t->assigned_worker, t->result, index, candidate);

  size_t n = send(serverfd, message, strlen(message), 0);
  printf("RESULT sent: %d bytes\n", (int)n);
}
// -------------------------------------- 

long long get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int validate_hash(char *candidate, char *target)
{
  
}