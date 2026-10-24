#include "network.h"
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  server_properties s = {0};
  in_port_t port;
  int heartbeat_ms;

  int thread_count;

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
  thread_count = atoi(args.threads);

  thread_range ranges[thread_count];
  pthread_t thread_ids[thread_count];

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

  // a loop to account for inital message
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
        // parse the message into variables
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

        break;
      }
    } else {
      buf[buf_index] = c;
      buf_index++;
    }
  }

  // calculate thread ranges
  divide_work(worker_t.start, worker_t.end, thread_count, ranges);

  // send start to the controller
  send_START(s.socket, &worker_t);
  worker_t.result = NOT_FOUND;

  // spawn threads
  pthread_mutex_init(&worker_t.mutex, NULL);
  thread_args t_args[thread_count];
  for (int i = 0; i < thread_count; i++) {

    t_args[i].length = worker_t.length;
    t_args[i].charset = charset;
    t_args[i].charset_size = charset_size;
    t_args[i].t_range = ranges[i];
    t_args[i].worker = &worker_t;
    t_args[i].thread_id = i;
    strcpy(t_args[i].hash, hash);

    if (pthread_create(&thread_ids[i], NULL, thread_worker, &t_args[i]) != 0) {
      printf("ERROR: pthread creation failed\n");
    }

    else {
      printf("THREAD_START: ID=%d subrange=[%d - %d]\n", i, ranges[i].start,ranges[i].end);
    }
  }

  long current_time = get_time_ms();
  int sent_result = 0;

  while (1) {
    struct pollfd pfd = {.fd = s.socket, .events = POLLIN};

    int ready = poll(&pfd, 1, 100);

    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }

      perror("poll");
      break;
    }

    if (ready > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
      ssize_t bytes = recv(s.socket, &c, 1, 0);

      if (bytes == 0) {
        printf("Controller disconnected\n");
        break;
      }

      if (bytes < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        }

        printf("ERROR: Couldn't recieve bytes\n");
        break;
      }

      if (c == '\n') {
        buf[buf_index] = '\0';
        printf("MSG: %s\n", buf);

        if (strncmp(buf, "COMPLETE", 8) == 0) {
          printf("Controller says task is complete. Exiting.\n");
          break;
        }

        buf_index = 0;
      } else {
        if (buf_index < sizeof(buf) - 1) {
          buf[buf_index++] = c;
        } else {
          printf("ERROR: Message too long");
        }
      }
    }

    // HEARTBEATS
    current_time = get_time_ms();
    long last_heartbeat;

    if (current_time - last_heartbeat >= heartbeat_ms) {
      send_HEARTBEAT(s.socket, &worker_t, 0);
      last_heartbeat = current_time;
    }

    if (worker_t.result == FOUND && sent_result == 0) {
      worker_t.result = FOUND;
      send_RESULT(s.socket, &worker_t, worker_t.index, worker_t.candidate);
      sent_result = 1;
    }

    else if (strncmp(buf, "COMPLETE", 8) == 0) {
      printf("Controller says task is complete. Exiting.\n");
      break;
    }
  }

  for (int i = 0; i < thread_count; i++) {
    pthread_join(thread_ids[i], NULL);
  }

  return EXIT_SUCCESS;
}
