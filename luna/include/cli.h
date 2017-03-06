#ifndef CLI_H
#define CLI_H

#define CLI_SERVICE_TOKENIZER       0x0001
#define CLI_SERVICE_SETTINGS        0x0002
#define CLI_SERVICE_WIFI            0x0004
#define CLI_SERVICE_AUTOUPDATE      0x0010
#define CLI_SERVICE_USB             0x0020
#define CLI_SERVICE_BLUETOOTH       0x0040
#define CLI_SERVICE_WEBSERVER       0x0080
#define CLI_SERVICE_INPUT           0x0100
#define CLI_SERVICE_TIMER           0x0800
#define CLI_SERVICE_TOUCHSCREEN     0x1000
#define CLI_SERVICE_ALL             0xFFFF

typedef struct {
  char *file;
  int (*execute)(const char *data);
} script_t;

typedef struct {
  script_t *scripts;
  int num_scripts;
  char **disabled_plugins;
  int num_disabled_plugins;
  int flags;
} arguments_t;

/*
 * Returns 1 if the options resulted in a script being executed, 0 otherwise.
 */
int cli_parse_options(arguments_t *arguments, int argc, char **argv);

#endif // CLI_H
