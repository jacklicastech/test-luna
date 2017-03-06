#include "util/jsmn_helpers.h"
#include "services.h"

jsmntok_t *jsmn_skip_node(jsmntok_t *json) {
  int len = json->size;
  int i;

  json++;
  for (i = 0; i < len; i++) {
    json = jsmn_skip_node(json);
  }

  return json;
}

int jsmn_object_iter(const char *json, jsmntok_t *token,
                     int (*iter)(const char *, jsmntok_t *, jsmntok_t *, void *),
                     void *arg) {
  if (token->type != JSMN_OBJECT) {
    LERROR("jsmn: token did not represent an object (%d)", token->type);
    return -1;
  }

  int num_objects = token->size;
  token++;
  for ((void) num_objects; num_objects > 0; num_objects--) {
    jsmntok_t *key   = token;
    jsmntok_t *value = ++token;
    int err = iter(json, key, value, arg);
    if (err) return err;
    token = jsmn_skip_node(token);
  }

  return 0;
}

int jsmn_array_iter(const char *json, jsmntok_t *token,
                    int (*iter)(const char *, jsmntok_t *, void *),
                    void *arg) {
  if (token->type != JSMN_ARRAY) {
    LERROR("jsmn: token did not represent an array (%d)", token->type);
    return -1;
  }

  int num_objects = token->size;
  token++;
  for ((void) num_objects; num_objects > 0; num_objects--) {
    jsmntok_t *element = token;
    int err = iter(json, element, arg);
    if (err) return err;
    token = jsmn_skip_node(token);
  }

  return 0;
}

