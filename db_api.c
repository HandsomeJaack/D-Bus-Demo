#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

int add_program(char *program, char *extension) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *m = NULL;
  sd_bus *bus = NULL;
  int s;
  int r;

  /* Connect to the system bus */
  r = sd_bus_open_user(&bus);
  if (r < 0) {
    fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
    goto finish;
  }
  r = sd_bus_call_method(bus, "net.handsome.Daemon", /* service to contact */
                         "/net/handsome/Daemon",     /* object path */
                         "net.handsome.Daemon",      /* interface name */
                         "Add",                      /* method name */
                         &error,     /* object to return error in */
                         &m,         /* return message on success */
                         "ss",       /* input signature */
                         program,    /* first argument */
                         extension); /* second argument */
  if (r < 0) {
    fprintf(stderr, "Failed to issue method call: %s\n", error.message);
    goto finish;
  }

  /* Parse the response message */
  r = sd_bus_message_read(m, "x", &s);
  if (r < 0) {
    fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
    goto finish;
  }

finish:
  sd_bus_error_free(&error);
  sd_bus_message_unref(m);
  sd_bus_unref(bus);
  return s;
}

int delete_program(char *program) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *m = NULL;
  sd_bus *bus = NULL;
  int s;
  int r;

  /* Connect to the system bus */
  r = sd_bus_open_user(&bus);
  if (r < 0) {
    fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
    goto finish;
  }
  r = sd_bus_call_method(bus, "net.handsome.Daemon", /* service to contact */
                         "/net/handsome/Daemon",     /* object path */
                         "net.handsome.Daemon",      /* interface name */
                         "Delete",                   /* method name */
                         &error,   /* object to return error in */
                         &m,       /* return message on success */
                         "s",      /* input signature */
                         program); /* first argument */
  if (r < 0) {
    fprintf(stderr, "Failed to issue method call: %s\n", error.message);
    goto finish;
  }

  /* Parse the response message */
  r = sd_bus_message_read(m, "x", &s);
  if (r < 0) {
    fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
    goto finish;
  }

finish:
  sd_bus_error_free(&error);
  sd_bus_message_unref(m);
  sd_bus_unref(bus);
  return s;
}

char **select_program(char *file) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *m = NULL;
  sd_bus *bus = NULL;
  char **list;
  int r;

  /* Connect to the system bus */
  r = sd_bus_open_user(&bus);
  if (r < 0) {
    fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
    goto finish;
  }
  r = sd_bus_call_method(bus, "net.handsome.Daemon", /* service to contact */
                         "/net/handsome/Daemon",     /* object path */
                         "net.handsome.Daemon",      /* interface name */
                         "Variants",                 /* method name */
                         &error, /* object to return error in */
                         &m,     /* return message on success */
                         "s",    /* input signature */
                         file);  /* first argument */
  if (r < 0) {
    fprintf(stderr, "Failed to issue method call: %s\n", error.message);
    goto finish;
  }

  r = sd_bus_message_read_strv(m, &list);
  if (r < 0) {
    fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
    goto finish;
  }

finish:
  sd_bus_error_free(&error);
  sd_bus_message_unref(m);
  sd_bus_unref(bus);
  /* Clear memory */
  char **ptr = list;
  while (*ptr) {
    free(*ptr++);
  }
  free(list);

  return list;
}

int run(char *program, char *file) {
  pid_t pid = fork();
  char *arg[3] = {program, file, 0};
  if (pid == 0) {
    execv(arg[0], arg);
  }
  return 0;
}