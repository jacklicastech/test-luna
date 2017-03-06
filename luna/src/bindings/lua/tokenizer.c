#define LUA_LIB

#include "config.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <pthread.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <string.h>

#include "services/logger.h"
#include "services/tokenizer.h"
#include "util/detokenize_template.h"
#include "util/lrc.h"
#include "util/luhn.h"
#include "util/base64_helpers.h"

#define MT_TOKEN  "MT_TOKEN"

/*
 * Returns a token ID with a value equal to that referenced by the token
 * string, or 0 on error.
 */
static token_id deserialize_token(const char *str) {
  // if it can't contain prefix and suffix then it can't contain a token
  if (strlen(str) < strlen(TOKEN_PREFIX) + strlen(TOKEN_SUFFIX))
    return 0;

  if (strncmp(str, TOKEN_PREFIX, strlen(TOKEN_PREFIX))) {
    LWARN("lua: tokenizer: this does not look like a token value: %s", str);
    return 0;
  }

  if (strcmp(str + strlen(str) - strlen(TOKEN_SUFFIX), TOKEN_SUFFIX)) {
    LWARN("lua: tokenizer: this does not look like a token value: %s", str);
    return 0;
  }

  return (token_id) atoll(str + strlen(TOKEN_PREFIX));
}

static int tokenizer_lrc(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  size_t len = strlen(str);
  char *detokenized = detokenize_template(str, &len);
  if (detokenized) {
    char result;
    result = lrc(detokenized, len);
    lua_pushlstring(L, &result, 1);
    free(detokenized);
    return 1;
  } else {
    LWARN("lua: tokenizer: can't calculate LRC: detokenization failed");
    return luaL_error(L, "lua: tokenizer: can't calculate LRC: detokenization failed");
  }
}

static int tokenizer_human(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  char *human = NULL;
  token_id token = deserialize_token(str);

  if (token_representation(token, &human))
    return 0; // no current token with this value, so let human be nil

  lua_pushstring(L, human);
  free(human);
  return 1;
}

static int tokenizer_free(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  token_id token = deserialize_token(str);
  lua_pushnumber(L, free_token(token));
  return 0;
}

static int tokenizer_nuke(lua_State *L) {
  lua_pushnumber(L, nuke_tokens());
  return 0;
}

static int tokenizer_extract_pan(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  char *x = NULL;
  size_t len = strlen(str);
  char *detokenized = detokenize_template(str, &len);

  if (detokenized) {
    for (x = detokenized; *x; x++) {
      if (*x == '=') {
        *x = '\0';
        token_id new_token = create_token(detokenized, strlen(detokenized) + 1, "card PAN");
        char *token_string = (char *) calloc(256, sizeof(char));
        sprintf(token_string, "%s%llu%s", TOKEN_PREFIX, (long long unsigned int) new_token, TOKEN_SUFFIX);
        lua_pushstring(L, token_string);
        free(detokenized);
        free(token_string);
        return 1;
      }
    }

    free(detokenized);
    return 0;
  } else {
    return luaL_error(L, "lua: tokenizer: can't extract PAN: detokenization failed");
  }
}

static int tokenizer_extract_expiry_date(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  char *x = NULL;
  size_t len = strlen(str);
  char *detokenized = detokenize_template(str, &len);

  if (detokenized) {
    for (x = detokenized; *x; x++) {
      if (*x == '=') {
        x++;
        int max = strlen(x) + 1;
        if (max > 4) max = 4;
        *(x + max) = '\0';
        token_id new_token = create_token(x, max + 1, "card expiry date");
        char *token_string = (char *) calloc(256, sizeof(char));
        sprintf(token_string, "%s%llu%s", TOKEN_PREFIX, (long long unsigned int) new_token, TOKEN_SUFFIX);
        lua_pushstring(L, token_string);
        free(detokenized);
        free(token_string);
        return 1;
      }
    }

    free(detokenized);
    return 0;
  } else {
    return luaL_error(L, "lua: tokenizer: can't extract PAN: detokenization failed");
  }
}

static int tokenizer_luhn(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  const char *x;
  size_t len = strlen(str);
  char *detokenized = detokenize_template(str, &len);

  if (detokenized) {
    // make sure it's numeric; allow a lua error, but not a C one
    for (x = detokenized; *x; x++)
      if ((*x) - '0' < 0 || (*x) - '0' > 9) {
        free(detokenized);
        luaL_error(L, "lua: tokenizer: can't perform Luhn test: input contains non-numeric characters");
      }

    lua_pushboolean(L, luhn(detokenized));
    free(detokenized);

    return 1;
  } else {
    return luaL_error(L, "lua: tokenizer: can't perform Luhn test: detokenization failed");
  }
}

static int tokenizer_base64_encode(lua_State *L) {
  (void) luaL_checkstring(L, 1);
  size_t len;
  const char *str = lua_tolstring(L, 1, &len);
  char *detokenized = detokenize_template(str, &len);
  if (detokenized) {
    LTRACE("lua: tokenizer: encoding %llu bytes as base64", (long long unsigned) len);
    char *base64 = base64_encode(detokenized, len);
    LINSEC("lua: tokenizer: base64 encoded data: %s", base64);
    token_id new_token = create_token(base64, strlen(base64) + 1, "base64-data");
    char *token_string = (char *) calloc(256, sizeof(char));
    sprintf(token_string, "%s%llu%s", TOKEN_PREFIX, (long long unsigned int) new_token, TOKEN_SUFFIX);
    lua_pushstring(L, token_string);

    free(detokenized);
    free(base64);
    free(token_string);

    return 1;
  } else {
    return 0;
  }
}

static int tokenizer_length(lua_State *L) {
  (void) luaL_checkstring(L, 1);
  size_t len;
  const char *str = lua_tolstring(L, 1, &len);
  char *detokenized = detokenize_template(str, &len);
  if (detokenized) {
    lua_pushnumber(L, len);
    free(detokenized);
  } else {
    lua_pushnumber(L, 0);
  }
  return 1;
}

// static const luaL_Reg token_methods[] = {
//   {"__gc",        tokenizer_free},
//   {"__tostring",  tokenizer_tostring},
//   {"id",          tokenizer_id},
//   {NULL,          NULL}
// };

static const luaL_Reg tokenizer_methods[] = {
  {"lrc",                 tokenizer_lrc},
  {"luhn",                tokenizer_luhn},
  {"base64_encode",       tokenizer_base64_encode},
  {"human",               tokenizer_human},
  {"free",                tokenizer_free},
  {"nuke",                tokenizer_nuke},
  {"length",              tokenizer_length},
  {"extract_expiry_date", tokenizer_extract_expiry_date},
  {"extract_pan",         tokenizer_extract_pan},
  {NULL,                  NULL}
};

LUALIB_API int luaopen_tokenizer(lua_State *L) {
  // luaL_newmetatable(L, MT_TOKEN);
  // lua_newtable(L);
  // luaL_setfuncs(L, token_methods, 0);
  // lua_setfield(L, -2, "__index");

  lua_newtable(L);
  luaL_setfuncs(L, tokenizer_methods, 0);

  return 1;
}

int init_tokenizer_lua(lua_State *L) {
  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package
  lua_pushcfunction(L, luaopen_tokenizer);
  lua_setfield(L, -2, "tokenizer");

  return 0;
}

void shutdown_tokenizer_lua(lua_State *L) {
  (void) L;
}
