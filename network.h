// network.h

#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
// #include <getopt.h>
#include <netinet/in.h>
// #include <string.h>
#include <sys/socket.h>

typedef struct {
  int socket;
  int client;
  int protocol;
  struct sockaddr_in addr;
  struct sockaddr_in6 addr6;
  socklen_t addr_len;
} server_properties;

struct ServerArgs {
  char *password;
  char *charset_file;
  char *timeout_ms;
  char *port_str;
};

struct ClientArgs {
  char *host;
  char *port_str;
  char *worker;
  char *heartbeat;
  char *threads;
};

typedef enum{
    PENDING,
    ASSIGNED,
    COMPLETE,
} TaskStatus;

typedef enum{
    WORKING,
    STALE,
} WorkerStatus;

typedef enum{
    FOUND,
    NOT_FOUND,
    NONE,
} TaskResult;

typedef struct {
    int task_id;
    int start;
    int end;
    int assigned_worker;
    int state;
    int result;
    int length;
    char *hash_value;
} task_t;

typedef struct {
    int worker_id;
    long last_heartbeat;
    int task_id;
    int progress;
    int state;
} worker_t;

int create_socket(server_properties *s);

int setup_server_address(server_properties *s, in_port_t *port);

int bind_socket(server_properties *s);

int start_listen(server_properties *s);

int connect_client(server_properties *s);

int valid_ip(const char *ip, server_properties *s);

int parse_port(char *portstr, in_port_t *p);

int validate_server_args(struct ServerArgs args, in_port_t *p);

int validate_client_args(struct ClientArgs args, in_port_t *p,
                         server_properties *s);

int parse_server_args(int argc, char *argv[], char **p, char **w, char **c,
                      char **t);

int parse_client_args(int argc, char *argv[], char **h, char **p, char **i,
                      char **t, char **e);

void send_ASSIGN(int clientfd, task_t *t);

void send_START(int serverfd, task_t *t);

void send_HEARTBEAT(int serverfd, task_t *t, int progress);

void send_RESULT(int serverfd, task_t *t, int index, char *candidate);

void send_COMPLETE(int clientfd, task_t *t);

long long get_time_ms(void);

#endif
