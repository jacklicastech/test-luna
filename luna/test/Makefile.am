AUTOMAKE_OPTIONS     = subdir-objects
ACLOCAL_AMFLAGS      = -I ../m4
AM_CFLAGS            = -g -fPIC -pthread -Wall -Werror -O0 -DJSMN_STRICT -DJSMN_PARENT_LINKS
AM_CXXFLAGS          = -g -fPIC -pthread -Wall -Werror -O0 -DJSMN_STRICT -DJSMN_PARENT_LINKS
AM_TESTS_ENVIRONMENT = export LUA_CPATH=../.libs/liblua_?.so;                \
                       export READ_PATHS=./fs_data:..;                       \
                       export WRITE_PATHS=.;                                 \
                       export PLUGINS_PATH=.libs;

TESTS                = bin/backend_service      bin/headers_parser           \
                       bin/lua_libxml_bindings  bin/lua_sqlite3_bindings     \
                       bin/settings_service     bin/tokenizer                \
                       bin/luhn                 bin/lua_tokenizer            \
                       bin/files                bin/encryption               \
                       bin/emv_helpers          bin/string_helpers           \
                       bin/tlv
check_PROGRAMS       = $(TESTS)
COMMON_CFLAGS        = -g -fPIC -pthread -Wall -Werror                       \
                       -DJSMN_STRICT -DJSMN_PARENT_LINKS                     \
                       -I./include -I../include -I.                          \
                       $(ZYRE_CFLAGS)                                        \
                       $(CZMQ_CFLAGS)                                        \
                       $(ZMQ_CFLAGS)                                         \
                       $(XML2_CFLAGS)                                        \
                       $(URIPARSER_CFLAGS)                                   \
                       $(LIBCTOSAPI_CFLAGS)                                  \
                       $(LUA_CFLAGS)                                         \
                       $(DBUS_CFLAGS)                                        \
                       $(CURL_CFLAGS)                                        \
                       $(LIBSSL_CFLAGS)                                      \
                       $(LIBCRYPTO_CFLAGS)                                   \
                       $(SQLITE3_CFLAGS)                                     \
                       $(DEVICE_CFLAGS)                                      \
                       $(CFLAGS) $(INC_DIR)
COMMON_LDADD         = $(ZYRE_LIBS)                                          \
                       $(CZMQ_LIBS)                                          \
                       $(ZMQ_LIBS)                                           \
                       $(XML2_LIBS)                                          \
                       $(LUA_LIBS)                                           \
                       $(URIPARSER_LIBS)                                     \
                       $(LIBCTOSAPI)                                         \
                       $(DBUS_LIBS)                                          \
                       $(CURL_LIBS)                                          \
                       $(LIBSSL_LIBS)                                        \
                       $(LIBCRYPTO_LIBS)                                     \
                       $(SQLITE3_LIBS)                                       \
                       -lstdc++                                              \
                       -ldl

bin_luhn_SOURCES = src/luhn_test.c                                           \
                   ../src/util/luhn.c
bin_luhn_CFLAGS = -g -fPIC -pthread -Wall -Werror -I./include -I../include   \
                  -I. $(DEVICE_CFLAGS) $(CFLAGS) $(INC_DIR)

bin_emv_helpers_SOURCES = src/emv_helpers_test.c                             \
                          ../src/services/logger.c                           \
                          ../src/services/settings.c                         \
                          ../src/util/files.c                                \
                          ../src/services/tokenizer.c                        \
                          ../src/util/detokenize_template.c                  \
                          ../src/util/base64_helpers.c                       \
                          ../src/util/migrator.c                             \
                          ../src/util/string_helpers.c                       \
                          ../src/util/encryption_helpers.c                   \
                          ../src/util/emv_helpers.c
bin_emv_helpers_CFLAGS = $(COMMON_CFLAGS)
bin_emv_helpers_LDADD  = $(COMMON_LDADD)

bin_string_helpers_SOURCES = src/string_helpers_test.c                       \
                             ../src/util/string_helpers.c
bin_string_helpers_CFLAGS = -g -fPIC -pthread -Wall -Werror -I./include      \
													  -I../include -I. $(DEVICE_CFLAGS) $(CFLAGS)      \
													  $(INC_DIR)

bin_files_SOURCES = src/files_test.c                                         \
                    ../src/util/files.c                                      \
                    ../src/util/migrator.c                                   \
                    ../src/services/events_proxy.c                           \
                    ../src/services/settings.c                               \
                    ../src/services/logger.c
bin_files_CFLAGS  = $(COMMON_CFLAGS)
bin_files_LDADD   = $(COMMON_LDADD)

bin_tlv_SOURCES = src/tlv_test.c                                             \
                  ../src/services/settings.c                                 \
                  ../src/services/logger.c                                   \
                  ../src/util/files.c                                        \
                  ../src/util/migrator.c                                     \
                  ../src/util/string_helpers.c                               \
                  ../src/util/tlv.c
bin_tlv_CFLAGS  = $(COMMON_CFLAGS)
bin_tlv_LDADD   = $(COMMON_LDADD)

bin_encryption_SOURCES = src/encryption_test.c                               \
                         ../src/util/encryption_helpers.c                    \
                         ../src/util/files.c                                 \
                         ../src/util/migrator.c                              \
                         ../src/services/settings.c                          \
                         ../src/services/events_proxy.c                      \
                         ../src/services/logger.c
bin_encryption_CFLAGS  = $(COMMON_CFLAGS)
bin_encryption_LDADD   = $(COMMON_LDADD)

# bin_autoupdate_SOURCES = src/autoupdate_slave_test.c                         \
#                          ../src/services/autoupdate_slave.c                  \
#                          ../src/services/autoupdate_parse_index.c            \
#                          ../src/services/events_proxy.c                      \
#                          ../src/services/logger.c                            \
#                          ../src/services/settings.c                          \
#                          ../src/util/curl_utils.c                            \
#                          ../src/util/jsmn.c                                  \
#                          ../src/util/jsmn_helpers.c                          \
#                          ../src/util/md5_helpers.c                           \
#                          ../src/util/migrator.c                              \
#                          ../src/util/files.c
# bin_autoupdate_CFLAGS  = $(COMMON_CFLAGS)
# bin_autoupdate_CXXFLAGS = $(COMMON_CFLAGS)
# bin_autoupdate_LDADD   = $(COMMON_LDADD)

bin_backend_service_SOURCES = src/backend_service_test.c                     \
															../src/plugin.c                                \
                              ../src/services/settings.c                     \
                              ../src/services/logger.c                       \
                              ../src/services/tokenizer.c                    \
                              ../src/services/events_proxy.c                 \
                              ../src/services/webserver.c                    \
                              ../src/util/base64_helpers.c                   \
                              ../src/util/curl_utils.c                       \
                              ../src/util/detokenize_template.c              \
                              ../src/util/encryption_helpers.c               \
                              ../src/util/files.c                            \
                              ../src/util/lrc.c                              \
                              ../src/util/machine_id.c                       \
                              ../src/util/migrator.c                         \
                              ../src/util/https_request.c                    \
                              ../src/util/headers_parser.c                   \
                              ../src/util/string_helpers.c                   \
                              ../src/util/tlv.c                              \
                              ../src/ssl_locks.c
bin_backend_service_CFLAGS = $(COMMON_CFLAGS)
bin_backend_service_CXXFLAGS = $(COMMON_CFLAGS)
bin_backend_service_LDADD = $(COMMON_LDADD)
bin_backend_service_LDFLAGS = -rdynamic


bin_headers_parser_SOURCES = src/headers_parser_test.c                       \
                             ../src/services/events_proxy.c                  \
                             ../src/services/logger.c                        \
                             ../src/services/settings.c                      \
                             ../src/util/files.c                             \
                             ../src/util/migrator.c                          \
                             ../src/util/headers_parser.c                    \
                             ../src/util/string_helpers.c
bin_headers_parser_CFLAGS = $(COMMON_CFLAGS)
bin_headers_parser_CXXFLAGS = $(COMMON_CFLAGS)
bin_headers_parser_LDADD = $(COMMON_LDADD)
bin_headers_parser_LDFLAGS = -rdynamic


bin_tokenizer_SOURCES = src/tokenizer_test.c                                 \
                        ../src/services/events_proxy.c                       \
                        ../src/services/logger.c                             \
                        ../src/services/settings.c                           \
                        ../src/services/tokenizer.c                          \
                        ../src/util/base64_helpers.c                         \
                        ../src/util/files.c                                  \
                        ../src/util/migrator.c                               \
                        ../src/util/detokenize_template.c                    \
                        ../src/util/encryption_helpers.c
bin_tokenizer_CFLAGS = $(COMMON_CFLAGS)
bin_tokenizer_LDADD = $(COMMON_LDADD)
bin_tokenizer_LDFLAGS = -rdynamic


bin_settings_service_SOURCES = src/settings_service_test.c                   \
                               ../src/services/events_proxy.c                \
                               ../src/util/files.c                           \
                               ../src/util/migrator.c                        \
                               ../src/services/logger.c                      \
                               ../src/services/settings.c
bin_settings_service_CFLAGS = $(COMMON_CFLAGS)
bin_settings_service_LDADD = $(COMMON_LDADD)
bin_settings_service_LDFLAGS = -rdynamic


bin_lua_libxml_bindings_SOURCES = src/lua_libxml_bindings_test.c             \
                                  ../src/plugin.c                            \
                                  ../src/services/events_proxy.c             \
                                  ../src/services/logger.c                   \
                                  ../src/services/settings.c                 \
                                  ../src/services/tokenizer.c                \
                                  ../src/bindings/lua.c                      \
                                  ../src/bindings/lua/ctos.c                 \
                                  ../src/bindings/lua/device.c               \
                                  ../src/bindings/lua/logger.c               \
                                  ../src/bindings/lua/printer.c              \
                                  ../src/bindings/lua/settings.c             \
                                  ../src/bindings/lua/services.c             \
                                  ../src/bindings/lua/timer.c                \
                                  ../src/bindings/lua/tokenizer.c            \
                                  ../src/bindings/lua/xml.c                  \
                                  ../src/bindings/lua/zmq.c                  \
                                  ../src/util/base64_helpers.c               \
                                  ../src/util/detokenize_template.c          \
                                  ../src/util/encryption_helpers.c           \
                                  ../src/util/files.c                        \
                                  ../src/util/lrc.c                          \
                                  ../src/util/luhn.c                         \
                                  ../src/util/machine_id.c                   \
                                  ../src/util/migrator.c
bin_lua_libxml_bindings_CFLAGS   = $(COMMON_CFLAGS)
bin_lua_libxml_bindings_LDADD    = $(COMMON_LDADD)
bin_lua_libxml_bindings_LDFLAGS  = -rdynamic


bin_lua_tokenizer_SOURCES = src/lua_tokenizer_test.c                         \
                            ../src/plugin.c                                  \
                            ../src/services/events_proxy.c                   \
                            ../src/services/logger.c                         \
                            ../src/services/settings.c                       \
                            ../src/services/tokenizer.c                      \
                            ../src/bindings/lua.c                            \
                            ../src/bindings/lua/ctos.c                       \
                            ../src/bindings/lua/device.c                     \
                            ../src/bindings/lua/logger.c                     \
                            ../src/bindings/lua/printer.c                    \
                            ../src/bindings/lua/settings.c                   \
                            ../src/bindings/lua/services.c                   \
                            ../src/bindings/lua/timer.c                      \
                            ../src/bindings/lua/tokenizer.c                  \
                            ../src/bindings/lua/xml.c                        \
                            ../src/bindings/lua/zmq.c                        \
                            ../src/util/base64_helpers.c                     \
                            ../src/util/detokenize_template.c                \
                            ../src/util/encryption_helpers.c                 \
                            ../src/util/files.c                              \
                            ../src/util/lrc.c                                \
                            ../src/util/luhn.c                               \
                            ../src/util/machine_id.c                         \
                            ../src/util/migrator.c
bin_lua_tokenizer_CFLAGS   = $(COMMON_CFLAGS)
bin_lua_tokenizer_LDADD    = $(COMMON_LDADD)
bin_lua_tokenizer_LDFLAGS  = -rdynamic


bin_lua_sqlite3_bindings_SOURCES = src/lua_sqlite3_bindings_test.c           \
                                   ../src/plugin.c                           \
                                   ../src/services/events_proxy.c            \
                                   ../src/services/logger.c                  \
                                   ../src/services/settings.c                \
                                   ../src/services/tokenizer.c               \
                                   ../src/bindings/lua.c                     \
                                   ../src/bindings/lua/ctos.c                \
                                   ../src/bindings/lua/device.c              \
                                   ../src/bindings/lua/logger.c              \
                                   ../src/bindings/lua/printer.c             \
                                   ../src/bindings/lua/settings.c            \
                                   ../src/bindings/lua/services.c            \
                                   ../src/bindings/lua/timer.c               \
                                   ../src/bindings/lua/tokenizer.c           \
                                   ../src/bindings/lua/xml.c                 \
                                   ../src/bindings/lua/zmq.c                 \
                                   ../src/util/base64_helpers.c              \
                                   ../src/util/detokenize_template.c         \
                                   ../src/util/encryption_helpers.c          \
                                   ../src/util/files.c                       \
                                   ../src/util/lrc.c                         \
                                   ../src/util/luhn.c                        \
                                   ../src/util/machine_id.c                  \
                                   ../src/util/migrator.c
bin_lua_sqlite3_bindings_CFLAGS   = $(COMMON_CFLAGS)
bin_lua_sqlite3_bindings_LDADD    = $(COMMON_LDADD)
bin_lua_sqlite3_bindings_LDFLAGS  = -rdynamic

