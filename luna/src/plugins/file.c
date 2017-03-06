/*
 * Exposes a module called `file` to Lua. Allows lua to read and write files
 * but only in a subdirectory called `files`.
 *
 * Only complete reads and writes are supported at this time. Examples:
 *
 *     local File = require('file')
 *     local content = File.read("path/to/file")
 *     File.write("path/to/file2", content)
 *
 */

#define LUA_LIB
#define _GNU_SOURCE
#include "config.h"
#include <string.h>
#include <stdio.h>
#include "services.h"
#include "lua.h"
#include "lauxlib.h"
#include "util/md5_helpers.h"
#include "util/files.h"

static char *strrstr(const char *haystack, const char *needle)
{
    if (*needle == '\0')
        return (char *) haystack;

    char *result = NULL;
    for (;;) {
        char *p = strstr(haystack, needle);
        if (p == NULL)
            break;
        result = p;
        haystack = p + 1;
    }

    return result;
}

/*
 * Read a file. Returns the content, or `nil` on an error.
 * 
 * Examples:
 * 
 *     File = require('file')
 *     content = File.read('path/to/file')
 *
 */
int file_read(lua_State *L) {
  int err = 0;
  char *filename = find_writable_file("files", lua_tostring(L, 1));
  char *out = malloc(1);
  LTRACE("reading from file %s", filename);

  FILE *fi = fopen(filename, "rb");
  if (!fi) {
    err = 2;
    goto cleanup_read;
  }

  size_t size = 0;
  char buf[1024];
  while (!feof(fi)) {
    int bytes_read = fread(buf, sizeof(buf), 1, fi);
    if (ferror(fi)) {
      err = 3;
      goto cleanup_read;
    }
    out = realloc(out, size + bytes_read);
    memcpy(out + size, buf, bytes_read);
    size += bytes_read;
  }


cleanup_read:
  if (fi) fclose(fi);
  if (err == 0)
    lua_pushlstring(L, out, size);
  else lua_pushnil(L);
  free(out);
  free(filename);
  return 1;
}

/*
 * Delete a file. Returns 0 on success.
 * 
 * Examples:
 * 
 *     File = require('file')
 *     File.rm('path/to/file')
 *
 */
int file_rm(lua_State *L) {
  char *path = find_writable_file("files", lua_tostring(L, 1));
  lua_pushnumber(L, remove(path));
  return 1;
}

/*
 * Generate an ETag for a specific file. Will return `nil` if the file could
 * not be read.
 * 
 * Examples:
 * 
 *     File = require('file')
 *     etag = File.etag('path/to/file')
 *
 */
int file_etag(lua_State *L) {
  char *path = find_writable_file("files", lua_tostring(L, 1));
  MD5_CTX ctx;
  char buf[1024];
  unsigned char digest[MD5_DIGEST_LENGTH];
  char actual_md5[MD5_DIGEST_LENGTH * 2];
  int i;
  FILE *file = NULL;
  int error = 0;

  file = fopen(path, "rb");
  if (!file) {
    LERROR("file: etag: couldn't open file %s for reading", path);
    free(path);
    return 0;
  }

  MD5_Init(&ctx);
  while (!error && !feof(file)) {
    memset(buf, 0, sizeof(buf));
    size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, file);
    if (bytes_read >= 0) {
      buf[bytes_read] = '\0';
      MD5_Update(&ctx, buf, bytes_read);
    }
    if (!feof(file) && bytes_read != sizeof(buf) - 1) {
      switch(errno) {
        case EAGAIN: // retry
          break;
        default:
          LERROR("file: etag: error '%s' while reading file %s", strerror(errno), path);
          error = 1;
          break;
      }
    }
  }
  MD5_Final(digest, &ctx);
  fclose(file);
  if (error) {
    free(path);
    return 0;
  }

  for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    sprintf(((char *)actual_md5) + i*2, "%02x", (unsigned int) digest[i]);

  lua_pushlstring(L, actual_md5, MD5_DIGEST_LENGTH * 2);
  return 1;
}

/*
 * Write to a file. Returns 0 on success.
 * 
 * Examples:
 * 
 *     File = require('file')
 *     err = File.write('path/to/file', 'content')
 *
 */
int file_write(lua_State *L) {
  int err = 0;
  char *filename = find_writable_file("files", lua_tostring(L, 1));
  char *dirname = strrstr(filename, "/");
  if (dirname == NULL) dirname = "files";
  else dirname = strndup(filename, dirname - filename);

  LTRACE("verifying subdirectory %s exists", dirname);
  if (mkdir_p(dirname)) {
    err = 1;
    goto cleanup_write;
  }

  size_t data_len;
  const char *data = lua_tolstring(L, 2, &data_len);

  LTRACE("writing %zu bytes to file %s", data_len, filename);
  FILE *fi = fopen(filename, "wb");
  if (!fi) {
    err = 2;
    goto cleanup_write;
  }

  if (fwrite(data, data_len, 1, fi) < 1) {
    err = 3;
  }

  fclose(fi);

cleanup_write:
  free(filename);
  free(dirname);
  lua_pushnumber(L, err);
  return 1;
}

static const luaL_Reg file_methods[] = {
  {"read",  file_read},
  {"write", file_write},
  {"etag",  file_etag},
  {"rm",    file_rm},
  {NULL,  NULL}
};

LUALIB_API int luaopen_file(lua_State *L) {
  lua_newtable(L);
  luaL_setfuncs(L, file_methods, 0);
  return 1;
}

void shutdown_file_lua(lua_State *L) {
  (void) L;
}
