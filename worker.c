#include "network.h"
#include <crypt.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
// #include <string.h>

int main(int argc, char *argv[]) {
  server_properties s = {0};
  in_port_t port;
  int heartbeat_ms;

  struct ClientArgs args = {0};

  if (parse_client_args(argc, argv, &args.host, &args.port_str, &args.worker,
                        &args.heartbeat, &args.threads) == -1) {
    return EXIT_FAILURE;
  }

  if (validate_client_args(args, &port, &s) == -1) {
    printf("ERROR: Failed to validate arguments\n");
    return EXIT_FAILURE;
  }

  heartbeat_ms = atoi(args.heartbeat);

  if (setup_server_address(&s, &port) == -1) {
    printf("ERROR: Failed to initialize server struct\n");
    return EXIT_FAILURE;
  }

  if (create_socket(&s) == -1) {
    printf("ERROR: Failed to create socket\n");
    return EXIT_FAILURE;
  }

  if (connect_client(&s) == -1) {
    printf("ERROR: Couldn't connect to server\n");
    return EXIT_FAILURE;
  }

  char buf[4096];
  int buf_index = 0;
  char c;

  task_t worker_t;

  int workerid;
  int task_id;
  int start;
  int end;
  int length;
  char hash[256];

  char charset[] = "abcdefghijklmnopqrstuvwxyz"
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "0123456789!@#$%^&*()-_=+[]{}:,.?";
  int charset_size = strlen(charset);

  // a loop to account for incomplete messages
  while (1) {

    // read in 1 by 1
    ssize_t bytes = recv(s.socket, &c, 1, 0);

    if (bytes < 0) {
      printf("ERROR: Failed to read in incoming byte\n");
      return EXIT_FAILURE;
    }

    // mark of complete message
    if (c == '\n') {
      buf[buf_index] = '\0';

      printf("MSG: %s\n", buf);
      buf_index = 0;

      if (strncmp(buf, "ASSIGN", 6) == 0) {
        // parse the message into variabls
        int successful_parse = sscanf(
            buf,
            "ASSIGN task=%d worker=%d start=%d end=%d length=%d hash=%255s",
            &task_id, &workerid, &start, &end, &length, hash);

        worker_t.assigned_worker = workerid;
        worker_t.task_id = task_id;
        worker_t.start = start;
        worker_t.end = end;
        worker_t.length = length;
        worker_t.result = NONE;

        printf("PARSE: %d\n", successful_parse);
        if (successful_parse != 6) {
          printf("ERROR: Invalid ASSIGN message\n");
          continue;
        }

        // send start to the controller
        send_START(s.socket, &worker_t);

        // search
        for (int i = start; i <= end; i++) {
          worker_t.result = NOT_FOUND;

          char candidate[256];
          int value = i;

          for (int pos = length - 1; pos >= 0; pos--) {
            candidate[pos] = charset[value % charset_size];
            value /= charset_size;
          }

          candidate[length] = '\0';

          struct crypt_data data = {0};
          char *candidate_hash = crypt_r(candidate, hash, &data);

          if (candidate_hash == NULL) {
            continue;
          }

          if (strcmp(candidate_hash, hash) == 0) {
            printf("FOUND CANDIDATE: %s\n", candidate);
            worker_t.result = FOUND;
            send_RESULT(s.socket, &worker_t, i, candidate);
          }

          // HEARTBEATS
          long current_time = get_time_ms();
          long last_heartbeat;

          if (current_time - last_heartbeat >= heartbeat_ms) {
            send_HEARTBEAT(s.socket, &worker_t, i);
            last_heartbeat = current_time;
          }
        }
      }

      else if (strncmp(buf, "COMPLETE", 8) == 0) {
        printf("Controller says task is complete. Exiting.\n");
        return EXIT_SUCCESS;
      }
    }

    else {
      buf[buf_index] = c;
      buf_index++;
    }
  }

  return EXIT_SUCCESS;
}
