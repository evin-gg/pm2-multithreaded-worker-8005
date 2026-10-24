#include "network.h"
#include <crypt.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define HASH_SETTING "$6$rounds=5000$comp8005$"

int main(int argc, char *argv[]) {

  // setup
  server_properties server = {0};
  in_port_t port;
  task_t task = {0};
  worker_t worker = {0};

  struct ServerArgs args = {0};
  server.protocol = AF_INET;

  // parse and validate args
  if (parse_server_args(argc, argv, &args.port_str, &args.password,
                        &args.charset_file, &args.timeout_ms) == -1) {
    return EXIT_FAILURE;
  }

  if (validate_server_args(args, &port) == -1) {
    printf("ERROR: Failed to validate arguments\n");
    return EXIT_FAILURE;
  }

  int timeout_ms = atoi(args.timeout_ms);

  // setup crypt
  struct crypt_data d = {0};
  char *hash = crypt_r(args.password, HASH_SETTING, &d);
  if (hash == NULL) {
    printf("ERROR: Failed to hash\n");
    return EXIT_FAILURE;
  }

  // read charset
  char buffer[512];
  size_t bytes_read;
  int charset_fd = open(args.charset_file, O_RDONLY);
  if (charset_fd < 0) {
    printf("ERROR: Failed to open file descriptor for charset_file");
    return EXIT_FAILURE;
  }

  int charset_len = 0;

  while ((bytes_read = read(charset_fd, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[bytes_read] = '\0';
    charset_len = (int)bytes_read - 1;
  }

  if (bytes_read < 0) {
    printf("ERROR: Failed to read charset_file\n");
    return EXIT_FAILURE;
  }

  // calculate range
  int pwd_len = strlen(args.password);
  int range = charset_len;

  for (int i = 0; i < pwd_len - 1; i++) {
    range *= charset_len;
  }

  if (create_socket(&server) == -1) {
    printf("ERROR: Socket creation failed");
    return EXIT_FAILURE;
  }

  // setup and start server
  if (setup_server_address(&server, &port) == -1) {
    printf("ERROR: Failed to initialize server struct\n");
    return EXIT_FAILURE;
  }

  if (bind_socket(&server)) {
    printf("ERROR: Failed to bind\n");
    return EXIT_FAILURE;
  }

  if (start_listen(&server) == -1) {
    printf("ERROR: Failed to start listening\n");
    return EXIT_FAILURE;
  }
  printf("Listening on port: %s\n", args.port_str);

  // setup the task
  task.task_id = 0;
  task.start = 0;
  task.end = range - 1;
  task.state = PENDING;
  task.result = NONE;
  task.length = pwd_len;
  task.hash_value = hash;

  char buf[4096];

  // int index;
  // char *candidate;

  while (1) {
    server.client = accept(server.socket, (struct sockaddr *)&server.addr,
                           &server.addr_len);

    if (server.client < 0) {
      printf("ERROR: Failed to accept client\n");
      continue;
    }

    // send the ASSIGN and create static worker state
    send_ASSIGN(server.client, &task);
    worker.worker_id = 0;
    worker.task_id = 0;
    worker.progress = 0;
    worker.state = -1;

    // loop of expected responses for start heartbeat
    char c;
    int buf_index;
    worker.last_heartbeat = get_time_ms();

    while (1) {

      // setup select
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(server.client, &readfds);

      struct timeval timeout;

      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;

      int ready = select(server.client + 1, &readfds, NULL, NULL, &timeout);

      if (ready < 0) {
        perror("select");
        return EXIT_FAILURE;
      }

      if (ready == 0) {

        long current_time = get_time_ms();

        if (current_time - worker.last_heartbeat >= timeout_ms) {
          printf("ERROR: Worker timed out\n");

          printf("STATUS: Task state PENDING\n");
          printf("STATUS: Worker state STALE\n");
          worker.state = STALE;
          task.state = PENDING;
          close(server.client);

          break;
        }

        continue;
      }

      // read in a full msg
      ssize_t bytes = recv(server.client, &c, 1, 0);

      if (bytes < 0) {
        printf("ERROR: Failed to read in incoming byte\n");
        return EXIT_FAILURE;
      }

      if (bytes == 0) {
        printf("ERROR: Worker disconnected\n");
        worker.state = STALE;
        task.state = PENDING;

        printf("STATUS: Task set to PENDING\n");
        printf("STATUS: Worker state STALE\n");

        close(server.client);
        break;
      }

      if (c == '\n') {
        buf[buf_index] = '\0';
        buf_index = 0;

        printf("MSG: %s\n", buf);

        if (strncmp(buf, "HEARTBEAT ", 9) == 0) {

          int task_id;
          int worker_id;
          int state;
          int progress;

          worker.last_heartbeat = get_time_ms();

          int parsed =
              sscanf(buf, "HEARTBEAT task=%d worker=%d state=%d progress=%d",
                     &task_id, &worker_id, &state, &progress);

          worker.progress = progress;

        } else if (strncmp(buf, "START ", 5) == 0) {
          worker.state = WORKING;
          task.state = ASSIGNED;
          printf("STATUS: Task state ASSIGNED\n");
          int task_id;
          int worker_id;

          int parsed =
              sscanf(buf, "START task=%d worker=%d", &task_id, &worker_id);

        } else if (strncmp(buf, "RESULT ", 6) == 0) {

          int task_id;
          int worker_id;
          int result;
          int index;
          char candidate[256];

          int parsed = sscanf(
              buf,
              "RESULT task=%d worker=%d result=%d index=%d candidate=%255s",
              &task_id, &worker_id, &result, &index, candidate);

          printf("--TASK RESULT--\n");
          printf("task: %d\n", task_id);
          printf("worker: %d\n", worker_id);
          printf("result: %d\n", result);
          printf("state: %d\n", task.state);
          printf("index: %d\n", index);
          printf("candidate: %s\n", candidate);

          if (result == FOUND) {

            if(strcmp(candidate, args.password) == 0)
            {
              printf("RESULT is valid\n");
              send_COMPLETE(server.client, &task);
            }

            
            close(server.socket);
            return EXIT_SUCCESS;
          }

        } else {
          printf("Unknown message: %s\n", buf);
        }

      } else {
        buf[buf_index] = c;
        buf_index++;
      }
    }
  };
}
