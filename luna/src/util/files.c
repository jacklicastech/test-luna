#include "config.h"
#define _GNU_SOURCE
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include "services/logger.h"

static const char *find_path_segment_end(const char *path) {
  const char *ch;
  for (ch = path; *ch; ch++) {
    if (*ch == FILE_PATH_SEPARATOR)
      return ch;
  }
  return ch;
}

/*
 * Converts a path to an absolute path. Relative paths are referenced from the
 * current working directory of the process unless dir_string is given, in
 * which case it will be used as the starting point. The given pathname may
 * start with a "~", which expands to the process owner’s home directory (the
 * environment variable HOME must be set correctly). "~user" expands to the
 * named user’s home directory.
 *
 * No check is performed on whether the path exists or is writable. If an
 * error occurs while expanding the path (for example, a home is referenced
 * but HOME is not set), `NULL` will be returned. Otherwise a freeable string
 * is returned.
 */
char *expand_path(const char *filename, const char *dir) {
  char result[PATH_MAX], unexpanded[PATH_MAX];
  const char *file = NULL;
  char *tmp1, *tmp2;

  memset(unexpanded, 0, sizeof(unexpanded));
  memset(result,     0, sizeof(result));

  if (filename[0] == '~') {
    const char *home = getenv("HOME");
    if (home == NULL) return NULL;
    if (filename[1] == FILE_PATH_SEPARATOR || filename[1] == '\0') {
      sprintf(unexpanded, "%s", home);
      file = NULL;
    } else {
      const char *segment_end = find_path_segment_end(filename + 1);
      size_t segment_length = (size_t) (segment_end - (filename + 1));
      tmp1 = strdup(home);
      tmp2 = strdup(dirname(tmp1));
      free(tmp1);
      snprintf(unexpanded, strlen(tmp2) + 2 + segment_length, "%s%c%s", tmp2, FILE_PATH_SEPARATOR, filename + 1);
      free(tmp2);
      file = *segment_end == '\0' ? NULL : segment_end + 1;
    }
  } else {
    char *cwd = get_current_dir_name();

    file = filename;
    if (dir != NULL) {
      // the `dir` parameter is expected to be trusted; we need to expand
      // it into an absolute path, but we don't need to worry about whether
      // it points somewhere safe.
      const char *segment_end = find_path_segment_end(dir + 1);
      if (!strncmp(dir, ".", segment_end - dir)) {
        // `dir` begins in CWD
        if (strlen(segment_end) > 1)
          sprintf(unexpanded, "%s%c%s", cwd, FILE_PATH_SEPARATOR, segment_end + 1);
        else
          sprintf(unexpanded, "%s", cwd);
      } else {
        strcpy(unexpanded, dir);
      }
      // remove trailing separator if necessary
      if (strlen(unexpanded) > 0) {
        if (unexpanded[strlen(unexpanded) - 1] == FILE_PATH_SEPARATOR)
          unexpanded[strlen(unexpanded) - 1] = '\0';
      }
    } else {
      sprintf(unexpanded, "%s", cwd);
    }

    free(cwd);
  }

  if (file != NULL)
    sprintf(unexpanded + strlen(unexpanded), "%c%s", FILE_PATH_SEPARATOR, file);

  const char *segment_end = unexpanded;
  const char *segment_start = unexpanded;
  char segment[PATH_MAX];
  while (*segment_end) {
    segment_end = find_path_segment_end(segment_end);
    memset(segment, 0, sizeof(segment));
    strncpy(segment, segment_start, (size_t) (segment_end - segment_start));
    
    if (!strcmp(segment, ".")) ;
    else if (!strcmp(segment, "")) ;
    else if (!strcmp(segment, "..")) {
      char *x;
      // need to remove the most recent segment from result
      // We can do this by finding the last path separator (or beginning of
      // string).
      for (x = result + strlen(result) - 1; x != (char *) result; x--) {
        if (*x == FILE_PATH_SEPARATOR) break;
      }
      memset(x, 0, strlen(x) * sizeof(char));
      if (strlen(result) == 0) *result = FILE_PATH_SEPARATOR;
    }
    else {
      result[strlen(result)] = FILE_PATH_SEPARATOR;
      strcat(result, segment);
    }

    // skip path separator unless we're at the end of the string
    if (*segment_end) segment_end++;
    segment_start = segment_end;
  }

  return strdup(result);
}

/*
 * Iterates over the paths specified in the READ_PATHS environment variable,
 * searching for the file specified in `filename`. If `subdir` is not `NULL`,
 * then the file *must* appear within `subdir`. Also, the file *must* be found
 * beneath one of the directories in `READ_PATHS`. This is helpful because
 * `filename` may contain data of questionable origin.
 *
 * For example, given a `READ_PATHS` equal to `./fs_data:/var/lib/myapp`,
 * calling `find_readable_file(NULL, "metrics.txt")` would result in either
 * `./fs_data/metrics.txt`, `/var/lib/myapp/metrics.txt`, or NULL if the file
 * exists in neither location.
 *
 * On the other hand, given the same `READ_PATHS`, a call to
 * `find_readable_file("insecure", "../secure/private_key.pem")` would
 * normally expand to `./fs_data/insecure/../secure/private_key.pem`, (which
 * is equivalent to `./fs_data/secure/private_key.pem`), or to
 * `/var/lib/myapp/secure/private_key.pem`. Given a non-`NULL` `subdir`,
 * this function guarantees that should this sort of expansion be attempted,
 * `NULL` will be returned instead. This way malicious or ignorant users will
 * be prevented from accessing files they are not authorized to.
 *
 * Both parameters must be `NULL`-terminated strings.
 *
 * The return value will be a freeable `NULL`-terminated string, or `NULL`.
 */
char *find_readable_file(const char *subdir, const char *filename) {
  char *paths = strdup(getenv("READ_PATHS") == NULL ? DEFAULT_READ_PATHS : getenv("READ_PATHS"));
  char *s = paths;
  char *p = NULL;
  char path[PATH_MAX];
  char join_subdir[PATH_MAX];
  char real_subdir[PATH_MAX];
  char real_subdir_filename[PATH_MAX];

  if (subdir == NULL) subdir = ".";

  do {
    p = strchr(s, MULTI_PATH_SEPARATOR);
    if (p != NULL) {
      p[0] = 0;
    }
    sprintf(path, "%s", s);
    s = p + 1;

    sprintf(join_subdir, "%s/%s", path, subdir);
    LTRACE("files-util: expanding realpath %s", join_subdir);
    if (realpath(join_subdir, real_subdir) == NULL) {
      continue;
    }

    sprintf(join_subdir, "%s/%s/%s", path, subdir, filename);
    LTRACE("files-util: expanding realpath %s", join_subdir);
    if (realpath(join_subdir, real_subdir_filename) == NULL) {
      continue;
    }

    // real_subdir_filename should contain real_subdir in full at offset 0,
    // if it's an authorized path.
    LTRACE("files-util: checking if %s starts with %s", real_subdir_filename, real_subdir);
    if (strstr(real_subdir_filename, real_subdir) != real_subdir_filename) {
      continue;
    }

    // Now, we just have to check whether it's a readable file.
    LDEBUG("files-util: expanded readable %s:%s into %s", subdir, filename, real_subdir_filename);
    if (!access(real_subdir_filename, R_OK)) {
      free(paths);
      return strdup(real_subdir_filename);
    }

  } while (p != NULL);
  free(paths);

  LWARN("files-util: could not expand %s:%s into a readable file", subdir, filename);
  return NULL;
}

/*
 * Functions exactly the same as `find_readable_file`, except that it checks
 * whether the file is writable, instead of readable; and that it works off
 * of the `WRITE_PATHS` variable instead of the `READ_PATHS` variable.
 */
char *find_writable_file(const char *subdir, const char *filename) {
  char *paths = strdup(getenv("WRITE_PATHS") == NULL ? DEFAULT_WRITE_PATHS : getenv("WRITE_PATHS"));
  char *s = paths;
  char *p = NULL;
  char path[PATH_MAX];
  char *real_subdir = NULL;
  char *real_subdir_filename = NULL;

  if (subdir == NULL) subdir = ".";

  do {
    p = strchr(s, MULTI_PATH_SEPARATOR);
    if (p != NULL) {
      p[0] = 0;
    }
    sprintf(path, "%s", s);
    s = p + 1;

    real_subdir = expand_path(subdir, path);
    if (real_subdir == NULL) continue;

    real_subdir_filename = expand_path(filename, real_subdir);
    if (real_subdir_filename == NULL) {
      free(real_subdir);
      continue;
    }

    // real_subdir_filename should contain real_subdir in full at offset 0,
    // if it's an authorized path.
    if (strstr(real_subdir_filename, real_subdir) != real_subdir_filename) {
      free(real_subdir);
      free(real_subdir_filename);
      continue;
    }

    free(real_subdir);
    free(paths);
    LDEBUG("files-util: expanded writable %s:%s into %s", subdir, filename, real_subdir_filename);
    return real_subdir_filename;
  } while (p != NULL);
  free(paths);
  return NULL;
}

// int rm_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
//  int result = remove(path);
//  if (result) {
//    LERROR("couldn't delete %s: %s", path, strerror(errno));
//  }
//  return result;
// }
void rm_cb(const char *path, const char *name, int type, int level, void *arg) {
  char full_path[PATH_MAX];
  sprintf(full_path, "%s/%s", path, name);
  int result = remove(full_path);
  if (result) {
    LERROR("couldn't delete %s: %s", path, strerror(errno));
  }
//    return result;
}

// FIXME apparently there's ftw and nftw for this, just use those instead.
void walkdir(const char *name, int level, void (*callback)(const char *path, const char *name, int type, int level, void *arg), void *arg) {
  DIR *dir;
  struct dirent *entry;
  
  // takes 2 passes, first files then dirs

  if (!(dir = opendir(name))) return;
  if (!(entry = readdir(dir))) { closedir(dir); return; }
  do {
    if (entry->d_type != DT_DIR) {
      callback(name, entry->d_name, entry->d_type, level + 1, arg);
    }
  } while ((entry = readdir(dir)));
  closedir(dir);
  
  if (!(dir = opendir(name))) return;
  if (!(entry = readdir(dir))) { closedir(dir); return; }
  do {
    if (entry->d_type == DT_DIR) {
      char path[1024];
      int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
      path[len] = 0;
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      callback(name, entry->d_name, entry->d_type, level + 1, arg);
      walkdir(path, level + 1, callback, arg);
    }
  } while ((entry = readdir(dir)));
  closedir(dir);
}

int rm_rf(const char *path) {
  walkdir(path, 0, rm_cb, NULL);
  return remove(path);
//    return nftw(path, rm_cb, 64, FTW_DEPTH | FTW_PHYS);
}

static void listdir_cb(const char *path, const char *name, int type, int level, void *arg) {
  if (type == DT_DIR)
    printf("%*s[%s]\n", level*2, "", name);
  else
    printf("%*s- %s\n", level*2, "", name);
}

void listdir(const char *name, int level) {
  walkdir(name, level, listdir_cb, NULL);
}

int mkdir_p(const char *dir) {
  char tmp[PATH_MAX];
  char *p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp),"%s",dir);
  len = strlen(tmp);
  if(tmp[len - 1] == '/')
    tmp[len - 1] = 0;
  for(p = tmp + 1; *p; p++)
    if(*p == '/') {
      *p = 0;
      if (mkdir(tmp, S_IRWXU) && errno != EEXIST) {
        return errno;
      }
      *p = '/';
    }
  if (mkdir(tmp, S_IRWXU) && errno != EEXIST)
    return errno;
  
  return 0;
}
