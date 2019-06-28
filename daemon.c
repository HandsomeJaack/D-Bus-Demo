#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

char *concat(char *s1, char *s2) {
  size_t len1 = strlen(s1);
  size_t len2 = strlen(s2);

  char *result = malloc(len1 + len2 + 1);

  if (!result) {
    fprintf(stderr, "malloc() failed: insufficient memory!\n");
    return NULL;
  }

  memcpy(result, s1, len1);
  memcpy(result + len1, s2, len2 + 1);

  return result;
}

/* Returns 0 if success, other value if failure" */
static int method_add(sd_bus_message *m, void *userdata,
                      sd_bus_error *ret_error) {
  (void)ret_error;
  (void)userdata;
  int r;
  char *path;
  char *extension;

  r = sd_bus_message_read(m, "ss", &path, &extension);
  if (r < 0) {
    fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    return r;
  }

  sqlite3 *db;
  char *err_msg = 0;

  int rc = sqlite3_open("extensions.db", &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return 1;
  }

  /* Glue sqllite command */
  char *create =
      "CREATE TABLE IF NOT EXISTS Programs(program_path TEXT,program_extension "
      "TEXT,"
      "UNIQUE(program_path, program_extension) ON CONFLICT IGNORE);"
      "INSERT INTO Programs VALUES('";
  char *add_path = concat(create, path);
  char *add_sep1 = concat(add_path, "', '");
  char *add_extension = concat(add_sep1, extension);
  char *sql = concat(add_extension, "');");

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
    sqlite3_close(db);

    return 1;
  }

  sqlite3_close(db);

  return sd_bus_reply_method_return(m, "x", 0);
}

/* Returns 0 if success, other value if failure" */
static int method_delete(sd_bus_message *m, void *userdata,
                         sd_bus_error *ret_error) {
  (void)ret_error;
  (void)userdata;
  int r;
  char *path;

  r = sd_bus_message_read(m, "s", &path);
  if (r < 0) {
    fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    return r;
  }

  sqlite3 *db;
  char *err_msg = 0;

  int rc = sqlite3_open("extensions.db", &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return 1;
  }

  char *del = "DELETE from Programs where program_path=";
  char *add_sep1 = concat(del, "'");
  char *add_path = concat(add_sep1, path);
  char *delete = concat(add_path, "';");

  rc = sqlite3_exec(db, delete, 0, 0, &err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
    sqlite3_close(db);

    return 1;
  }

  sqlite3_close(db);

  return sd_bus_reply_method_return(m, "x", 0);
}

/* Returns array of string if success, empty array if no extension in database
 */
static int method_variants(sd_bus_message *m, void *userdata,
                           sd_bus_error *ret_error) {
  (void)ret_error;
  (void)userdata;
  int r;
  char *file;

  r = sd_bus_message_read(m, "s", &file);
  if (r < 0) {
    fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    return r;
  }

  char *format = strrchr(
      file, '.'); /* Detachment of necessary extension from full file name */
  sqlite3 *db;
  sqlite3_stmt *res;

  int rc = sqlite3_open("extensions.db", &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return 1;
  }

  char *sql = "SELECT program_path FROM Programs WHERE program_extension=?";

  rc =
      sqlite3_prepare_v2(db, sql, -1, &res, 0); /* Precompilation sql commamd */

  if (rc != SQLITE_OK) {
    printf("error: %s!\n", sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_bind_text(res, 1, format, -1,
                    0); /* Adding @format@ paramater to search request */
  char **final = malloc(0);
  int size = 0;
  for (;;) {
    rc = sqlite3_step(res); /* Iterating over results */
    if (rc == SQLITE_DONE) break;
    if (rc != SQLITE_ROW) {
      printf("error: %s!\n", sqlite3_errmsg(db));
      break;
    }
    /* Adding new search result into array, works like push_back in vector*/
    final = realloc(final, (size + 1) * sizeof *final);
    final[size] = strdup((const char *)sqlite3_column_text(res, 0));
    size++;
  }

  final = realloc(final, (size + 1) * sizeof *final);
  final[size] = 0;

  sqlite3_finalize(res);
  sqlite3_close(db);
  /* Creating new message to return where our array will be handeled */
  sd_bus_message *n = 0;
  sd_bus_message_new_method_return(m, &n);
  sd_bus_message_append_strv(n, final);

  /* Clear memory */
  char **ptr = final;
  while (*ptr) {
    free(*ptr++);
  }
  free(final);
  return sd_bus_send(NULL, n, NULL);
}

/* Descroption of daemon methods */
static const sd_bus_vtable daemon_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Add", "ss", "x", method_add, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Delete", "s", "x", method_delete,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Variants", "s", "as", method_variants,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

/* Daemon initialization */
int main() {
  sd_bus_slot *slot = NULL;
  sd_bus *bus = NULL;
  int r;

  r = sd_bus_open_user(&bus);
  if (r < 0) {
    fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
    goto finish;
  }

  r = sd_bus_add_object_vtable(bus, &slot,
                               "/net/handsome/Daemon", /* object path */
                               "net.handsome.Daemon",  /* interface name */
                               daemon_vtable, NULL);
  if (r < 0) {
    fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
    goto finish;
  }

  r = sd_bus_request_name(bus, "net.handsome.Daemon", 0);
  if (r < 0) {
    fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
    goto finish;
  }

  for (;;) {
    /* Process requests */
    r = sd_bus_process(bus, NULL);
    if (r < 0) {
      fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
      goto finish;
    }
    if (r >
        0) /* we processed a request, try to process another one, right-away */
      continue;

    /* Wait for the next request to process */
    r = sd_bus_wait(bus, (uint64_t)-1);
    if (r < 0) {
      fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
      goto finish;
    }
  }

finish:
  sd_bus_slot_unref(slot);
  sd_bus_unref(bus);

  return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}