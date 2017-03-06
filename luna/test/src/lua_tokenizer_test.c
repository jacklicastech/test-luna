#define  _GNU_SOURCE
#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/limits.h>
#include "services.h"
#include "bindings.h"
#include "util/encryption_helpers.h"

int err = 0;

#define ASSERT(x) if (!(x)) { LERROR("FAIL: " #x); err++; return; }

void test_lrc(void) {
  ASSERT(!lua_run_script(
    "tok = require('tokenizer')"                                          "\n"
    "assert(tok.lrc(\"\\x8a\\x3d\")                     == \"\xb7\")"     "\n"
    "assert(tok.lrc(\"\\x00\")                          == \"\\x00\")"    "\n"
    "assert(tok.lrc(\"\\xfb\\xd4\")                     == \"\\x2f\")"    "\n"
    "assert(tok.lrc(\"\\xd9\\x15\")                     == \"\\xcc\")"    "\n"
    "assert(tok.lrc(\"\\x8a\\x3d\\xfb\\xd4\\xd9\\x15\") == \"\\x54\")"    "\n"
  ));
}

void test_base64(void) {
  char script[2048];

  // ensure we don't lose the NUL if a binary string contains one
  ASSERT(!lua_run_script(
    "tok = require('tokenizer')"                                          "\n"
    "r = tok.base64_encode(\"\\x00\")"                                    "\n"
    "assert(tok.length(r) == 4, 'expected token length 4, got: ' .. tostring(tok.length(r)))"
  ));

  // Ensure the token data matches an encoded NULL
  // FIXME how to get the token ID from lua?
  // assert(!strcmp(str, "AA=="));

  token_id token = create_token("secret", strlen("secret"), "shhh");
  sprintf(script, "tok = require('tokenizer')"                     "\n"
                  "len = tok.length(tok.base64_encode('%s%u%s'))"              "\n"
                  "assert(len == 8, 'expected token length 8, got: ' .. tostring(len))",
          TOKEN_PREFIX, token, TOKEN_SUFFIX);
  ASSERT(!lua_run_script(script));
}

void test_luhn(void) {
  ASSERT(!lua_run_script("tok = require('tokenizer')"                   "\n"
                         "assert(tok.luhn('49927398716')      == true)" "\n"
                         "assert(tok.luhn('49927398717')      == false)""\n"
                         "assert(tok.luhn('1234567812345678') == false)""\n"
                         "assert(tok.luhn('1234567812345670') == true)" "\n"
                         "assert(pcall(tok.luhn, 'a')         == false)""\n"));
}

void test_human(void) {
  char script[2048];
  token_id token;

  token = create_token("secret", strlen("secret"), "shhh");
  sprintf(script, "tok = require('tokenizer')"                     "\n"
                  "assert(tok.human('%s%u%s') == 'shhh')",
          TOKEN_PREFIX, token, TOKEN_SUFFIX);

  ASSERT(!lua_run_script(script));
}

void test_free(void) {
  char script[2048];
  token_id token;

  token = create_token("secret", strlen("secret"), "shhh");
  sprintf(script, "tok = require('tokenizer')"                     "\n"
                  "tok.free('%s%u%s')"                             "\n"
                  "assert(tok.human('%s%u%s') == nil)",
          TOKEN_PREFIX, token, TOKEN_SUFFIX,
          TOKEN_PREFIX, token, TOKEN_SUFFIX);

  ASSERT(!lua_run_script(script));
}

void test_nuke(void) {
  char script[2048];
  token_id token;

  token = create_token("secret", strlen("secret"), "shhh");
  sprintf(script, "tok = require('tokenizer')"                     "\n"
                  "tok.nuke()"                                     "\n"
                  "assert(tok.human('%s%u%s') == nil)",
          TOKEN_PREFIX, token, TOKEN_SUFFIX);

  ASSERT(!lua_run_script(script));
}

int main(int argc, char **argv) {
  init_logger_service(LOG_LEVEL_INSEC);
  if (init_tokenizer_service()) return 1;
  if (init_encryption())        return 1;

  #define RUN_TEST(x) LINFO("Beginning " #x); x();
  RUN_TEST(test_lrc);
  RUN_TEST(test_base64);
  RUN_TEST(test_luhn);
  RUN_TEST(test_human);
  RUN_TEST(test_free);
  RUN_TEST(test_nuke);

  shutdown_tokenizer_service();
  shutdown_logger_service();

  return err;
}
