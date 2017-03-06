#include "services.h"
#include "util/files.h"
#include "services/autoupdate_internal.h"

static zsock_t *backend;
static zactor_t *slave;

bool ALLOW_DISABLE_SSL_VERIFICATION = false;
char cacerts_bundle[PATH_MAX];
zsock_t *bcast = NULL;

/*
 * Fetch the index, simulate a 404 error on the backend, and check the result.
 */
int test_missing_index(void) {
  int code, err = 0;
  char *message = NULL;
  zmsg_t *req = NULL;

  zsock_send(slave, "ss", "fetch-index", "https://s3.amazonaws.com/appilee/tip/luna/mp200/index.json");
  req = zmsg_recv(backend);
  zsock_send(backend, "ss", "broadcast_id", "1");
  zsock_send(bcast,   "ssssissss", "1", "result", "error", "code", 404, "message", "file not found", "duration", "0.0");
  zsock_recv(slave, "is", &code, &message);

  LINFO("test_missing_index: %d, %s", code, message);
  if (code != 404) {
    LERROR("expected code 404, got: %d", code);
    err = 1;
  }
  if (strcmp(message, "file not found")) {
    LERROR("expected 'file not found', got: %s", message);
    err = 1;
  }

  zmsg_destroy(&req);
  free(message);
  return err;
}

/*
 * Test that an index JSON can be parsed into a consumable data structure.
 */
int test_index_parser(void) {
#define ASSERT(cond) if (!(cond)) { LERROR("\x1b[31mFAIL\x1b[0m: " #cond); return 1; } else LINFO("PASS: " #cond);
  update_t *update;

  update = autoupdate_parse_index("{}", "endpoint", "bucket", "prefix");
  ASSERT(update            != NULL);
  ASSERT(update->num_files == 0);
  ASSERT(update->files     == NULL);
  autoupdate_free(&update);

  update = autoupdate_parse_index("{\"file\": [{\"etag\":\"1234\"}]}", "endpoint", "bucket", "prefix");
  ASSERT(update            != NULL);
  ASSERT(update->num_files == 1);
  ASSERT(update->files     != NULL);
  ASSERT(update->files[0].num_releases == 1);
  ASSERT(update->files[0].releases != NULL);
  ASSERT(update->files[0].releases[0].num_notes == 0);
  ASSERT(update->files[0].releases[0].notes == NULL);
  autoupdate_free(&update);
  
  update = autoupdate_parse_index("{\"file\": [{\"etag\":\"cbb49694b2eea180bfca3f782e2825e6\",\"release_notes\":[]}]}", "endpoint", "bucket", "prefix");
  ASSERT(update            != NULL);
  ASSERT(update->num_files == 1);
  ASSERT(update->files     != NULL);
  ASSERT(update->files[0].num_releases == 1);
  ASSERT(update->files[0].releases != NULL);
  ASSERT(!strncmp(update->files[0].releases[0].md5, "cbb49694b2eea180bfca3f782e2825e6", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[0].releases[0].num_notes == 0);
  ASSERT(update->files[0].releases[0].notes == NULL);
  autoupdate_free(&update);
  
  update = autoupdate_parse_index("{\"file\": [{\"etag\":\"cbb49694b2eea180bfca3f782e2825e6\",\"release_notes\":[\"one\",\"two\"]}]}", "endpoint", "bucket", "prefix");
  ASSERT(update            != NULL);
  ASSERT(update->num_files == 1);
  ASSERT(update->files     != NULL);
  ASSERT(update->files[0].num_releases == 1);
  ASSERT(update->files[0].releases != NULL);
  ASSERT(!strncmp(update->files[0].releases[0].md5, "cbb49694b2eea180bfca3f782e2825e6", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[0].releases[0].num_notes == 2);
  ASSERT(update->files[0].releases[0].notes != NULL);
  ASSERT(!strcmp(update->files[0].releases[0].notes[0], "one"));
  ASSERT(!strcmp(update->files[0].releases[0].notes[1], "two"));
  autoupdate_free(&update);
  
  update = autoupdate_parse_index(
              "{\"file\": [{"
                "\"etag\":\"cbb49694b2eea180bfca3f782e2825e6\","
                "\"release_notes\":[\"one\",\"two\"]"
              "},{"
                "\"etag\":\"effbcb2f11fc53f5995378850480279c\","
                "\"release_notes\":[\"three\",\"four\"]"
              "}]}", "endpoint", "bucket", "prefix");
  ASSERT(update            != NULL);
  ASSERT(update->num_files == 1);
  ASSERT(update->files     != NULL);
  ASSERT(update->files[0].num_releases == 2);
  ASSERT(update->files[0].releases != NULL);
  ASSERT(!strncmp(update->files[0].releases[0].md5, "cbb49694b2eea180bfca3f782e2825e6", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[0].releases[0].num_notes == 2);
  ASSERT(update->files[0].releases[0].notes != NULL);
  ASSERT(!strcmp(update->files[0].releases[0].notes[0], "one"));
  ASSERT(!strcmp(update->files[0].releases[0].notes[1], "two"));
  ASSERT(!strncmp(update->files[0].releases[1].md5, "effbcb2f11fc53f5995378850480279c", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[0].releases[1].num_notes == 2);
  ASSERT(update->files[0].releases[1].notes != NULL);
  ASSERT(!strcmp(update->files[0].releases[1].notes[0], "three"));
  ASSERT(!strcmp(update->files[0].releases[1].notes[1], "four"));
  autoupdate_free(&update);
  
  update = autoupdate_parse_index(
              "{\"file1\": [{"
                "\"etag\":\"cbb49694b2eea180bfca3f782e2825e6\","
                "\"release_notes\":[\"one\",\"two\"]"
              "},{"
                "\"etag\":\"effbcb2f11fc53f5995378850480279c\","
                "\"release_notes\":[\"three\",\"four\"]"
              "}], \"file2\": [{"
                "\"etag\":\"5c7d27c14433ee340944a6a642f2fa7f\","
                "\"release_notes\":[\"five\"]"
              "}]}", "endpoint", "bucket", "prefix");
  ASSERT(update            != NULL);
  ASSERT(update->num_files == 2);
  ASSERT(update->files     != NULL);
  ASSERT(!strcmp(update->files[0].relative_path, "file1"));
  ASSERT(!strcmp(update->files[0].url, "https://endpoint/bucket/prefix/file1"));
  ASSERT(update->files[0].num_releases == 2);
  ASSERT(update->files[0].releases != NULL);
  ASSERT(!strncmp(update->files[0].releases[0].md5, "cbb49694b2eea180bfca3f782e2825e6", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[0].releases[0].num_notes == 2);
  ASSERT(update->files[0].releases[0].notes != NULL);
  ASSERT(!strcmp(update->files[0].releases[0].notes[0], "one"));
  ASSERT(!strcmp(update->files[0].releases[0].notes[1], "two"));
  ASSERT(!strncmp(update->files[0].releases[1].md5, "effbcb2f11fc53f5995378850480279c", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[0].releases[1].num_notes == 2);
  ASSERT(update->files[0].releases[1].notes != NULL);
  ASSERT(!strcmp(update->files[0].releases[1].notes[0], "three"));
  ASSERT(!strcmp(update->files[0].releases[1].notes[1], "four"));
  ASSERT(!strcmp(update->files[1].relative_path, "file2"));
  ASSERT(!strcmp(update->files[1].url, "https://endpoint/bucket/prefix/file2"));
  ASSERT(update->files[1].num_releases == 1);
  ASSERT(update->files[1].releases != NULL);
  ASSERT(!strncmp(update->files[1].releases[0].md5, "5c7d27c14433ee340944a6a642f2fa7f", MD5_DIGEST_LENGTH*2));
  ASSERT(update->files[1].releases[0].num_notes == 1);
  ASSERT(update->files[1].releases[0].notes != NULL);
  ASSERT(!strcmp(update->files[1].releases[0].notes[0], "five"));
  autoupdate_free(&update);
  
  return 0;
}

/*
 * Fetch the index. Indicate success, and return the body.
 */
int test_found_index(void) {
  int code, err = 0;
  char *message = NULL;
  zmsg_t *req = NULL;

  zsock_send(slave, "ss", "fetch-index", "https://s3.amazonaws.com/appilee/tip/luna/mp200/index.json");
  req = zmsg_recv(backend);
  zsock_send(backend, "ss", "broadcast_id", "2");
  zsock_send(bcast, "ssssissss", "2", "result", "error", "code", 200, "message", "{}", "duration", "0.0");
  zsock_recv(slave, "is", &code, &message);

  LINFO("test_found_index: %d, %s", code, message);
  if (code != 200) {
    LERROR("expected code 200, got: %d", code);
    err++;
  }
  if (strcmp(message, "{}")) {
    LERROR("expected '{}', got: %s", message);
    err++;
  }

  zmsg_destroy(&req);
  free(message);
  return err;
}

/*
 * Download the specified file and verify its integrity.
 */
int test_download_and_verify_file(void) {
  int code, err = 0;
  char *message = NULL;
  char url[PATH_MAX];

  // find the test file as an absolute path
  char *path = find_readable_file(NULL, "hello.txt");
  assert(path);
  sprintf(url, "file://%s", path);
  free(path);

  // so, some internal knowledge of how auoupdate performs download-and-verify
  // is necessary. Unlike fetch-index, which delegates to backend, download
  // will use curl directly. The reason for this is that the index is not
  // expected to be large, and will fit in memory, but the update files
  // themselves could grow quite large and should be saved to files. Anyway,
  // they need to be saved regardless in order to be installed. The point is,
  // backend does not use files and augmenting it to accept an option to save
  // files would be dangerous because the same message format is exposed to
  // lua and would provide a vector for lua to gain unfettered filesystem
  // access.

  path = find_writable_file(NULL, "downloaded-hello.txt");
  zsock_send(slave, "ssss", "download-and-verify", url, path, "952d2c56d0485958336747bcdd98590d");
  zsock_recv(slave, "is", &code, &message);
  LINFO("test_download_and_verify_file success: %d, %s", code, message);
  if (code != 0) {
    LERROR("expected code 0, got: %d", code);
    err++;
  }
  free(message);

  // now try a checksum failure
  zsock_send(slave, "ssss", "download-and-verify", url, path, "952d2c56d0485958336747bcddffffff");
  zsock_recv(slave, "is", &code, &message);
  LINFO("test_download_and_verify_file fail: %d, %s", code, message);
  if (code == 0) {
    LERROR("expected code != 0, got: %d", code);
    err++;
  }
  if (strcmp(message, "checksum does not match")) { LERROR("expected 'checksum does not match', got: %s", message); err = 1; }
  free(message);

  free(path);
  return err;
}

/*
 * Populates the file to be downloaded with a string different from 'hello',
 * and passes an MD5 that matches the different string. This should indicate
 * that the local version of the file is accurate from a checksum perspective,
 * and therefore the remote file should not be downloaded.
 * 
 * After ensuring a success from the AU slave, the file is examined to verify
 * that it was not downloaded.
 *
 * Use case: a file was downloaded but a subsequent file failed to download.
 * There's no need to download it again because we already have a matching
 * checksum, and therefore an intact file.
 */
int test_only_verify_when_download_is_unnecessary(void) {
  int code, err = 0;
  char *message = NULL;
  char url[PATH_MAX];
  char buf[1024];
  FILE *file;

  // find the test file as an absolute path
  char *path = find_readable_file(NULL, "hello.txt");
  assert(path);
  sprintf(url, "file://%s", path);
  free(path);

  // prepare the 'downloaded' file
  path = find_writable_file(NULL, "downloaded-hello.txt");
  file = fopen(path, "wb");
  assert(file);
  fprintf(file, "this is not hello");
  fclose(file);

  // perform the test
  zsock_send(slave, "ssss", "download-and-verify", url, path, "b8d8761cb5ba83e0849a27c66b89d4f7");
  zsock_recv(slave, "is", &code, &message);
  LINFO("test_only_verify_when_download_is_unnecessary: %d, %s", code, message);
  if (code != 0) {
    LERROR("expected code 0, got: %d", code);
    err++;
  }
  free(message);

  // check if the file was overwritten
  file = fopen(path, "rb");
  assert(file);
  memset(buf, 0, 1024);
  assert(fread(buf, 1, 1024, file) > 0);
  if (strcmp(buf, "this is not hello")) {
    LERROR("expected file to not be modified, but it was: %s", buf);
    err++;
  }
  fclose(file);

  free(path);
  return err;
}

int main(int argc, char **argv) {
  int err = 0;

  err = err || init_logger_service(LOG_LEVEL_DEBUG)
            || init_settings_service();
  assert(bcast   = zsock_new_pub(">" EVENTS_SUB_ENDPOINT));
  assert(backend = zsock_new_rep("inproc://backend"));
  assert(slave   = zactor_new(autoupdate_slave, NULL));

  char *cacerts = find_readable_file(NULL, "cacerts.pem");
  if (!cacerts) {
    LWARN("CA certs bundle could not be loaded: backend connections will fail");
    cacerts_bundle[0] = '\0';
  } else {
    sprintf(cacerts_bundle, "%s", cacerts);
    free(cacerts);
  }

  if (!err) {
    err += test_missing_index();
    err += test_found_index();
    err += test_index_parser();
    err += test_download_and_verify_file();
    err += test_only_verify_when_download_is_unnecessary();
  }

  zactor_destroy(&slave);
  shutdown_settings_service();
  shutdown_logger_service();
  zsock_destroy(&backend);
  zsock_destroy(&bcast);

  return err;
}
