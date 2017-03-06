AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS  = -I m4
SUBDIRS          = test
bin_PROGRAMS     = luna
AM_CFLAGS        = -g -fPIC -pthread -Wall -Werror -DJSMN_STRICT -DJSMN_PARENT_LINKS
AM_CXXFLAGS      = -g -fPIC -pthread -Wall -Werror -DJSMN_STRICT -DJSMN_PARENT_LINKS
lib_LTLIBRARIES  = liblua_lsqlite3.la libpin_entry.la libkeypad.la \
                   libemv_contact.la libemv_contactless.la libbackend.la \
                   libethernet.la librs232.la libdatetime.la libsocket.la \
                   libfile.la libautoupdate.la

if HAVE_CTOS
include m4/castles.am
endif

COMMON_CFLAGS = -g -fPIC -pthread -Wall -Werror  -rdynamic                   \
                -DJSMN_STRICT -DJSMN_PARENT_LINKS                            \
                -I./include                                                  \
                $(ZYRE_CFLAGS)                                               \
                $(CZMQ_CFLAGS)                                               \
                $(ZMQ_CFLAGS)                                                \
                $(XML2_CFLAGS)                                               \
                $(URIPARSER_CFLAGS)                                          \
                $(LIBCTOSAPI_CFLAGS)                                         \
                $(LUA_CFLAGS)                                                \
                $(DBUS_CFLAGS)                                               \
                $(CURL_CFLAGS)                                               \
                $(LIBSSL_CFLAGS)                                             \
                $(LIBCRYPTO_CFLAGS)                                          \
                $(SQLITE3_CFLAGS)                                            \
                $(DEVICE_CFLAGS)                                             \
                $(CFLAGS) $(INC_DIR)
COMMON_LDADD = $(ZYRE_LIBS) $(CZMQ_LIBS) $(ZMQ_LIBS) $(XML2_LIBS)            \
               $(LUA_LIBS) $(URIPARSER_LIBS)                                 \
               $(LIBCTOSAPI) $(LIBCAEMVL2) $(LIBCACLENTRY)                   \
               $(DBUS_LIBS)                                                  \
               $(CURL_LIBS) $(LIBSSL_LIBS) $(LIBCRYPTO_LIBS) $(SQLITE3_LIBS) \
               $(LIBS)                                                       \
               -ldl
COMMON_LDFLAGS = -rdynamic

liblua_lsqlite3_la_SOURCES = src/bindings/lua/lsqlite3.c
liblua_lsqlite3_la_CFLAGS  = $(COMMON_CFLAGS)
liblua_lsqlite3_la_LIBADD  = $(COMMON_LDADD)
liblua_lsqlite3_la_LDFLAGS = $(COMMON_LDFLAGS)

libsocket_la_SOURCES = src/plugins/socket.c
libsocket_la_CFLAGS  = $(COMMON_CFLAGS)
libsocket_la_LIBADD  = $(COMMON_LDADD)
libsocket_la_LDFLAGS = $(COMMON_LDFLAGS)

libautoupdate_la_SOURCES = src/plugins/autoupdate.c                 \
													 src/plugins/autoupdate_install_castles.c \
													 src/plugins/autoupdate_install_debian.c
libautoupdate_la_CFLAGS  = $(COMMON_CFLAGS)
libautoupdate_la_LIBADD  = $(COMMON_LDADD)
libautoupdate_la_LDFLAGS = $(COMMON_LDFLAGS)

libfile_la_SOURCES = src/plugins/file.c
libfile_la_CFLAGS  = $(COMMON_CFLAGS)
libfile_la_LIBADD  = $(COMMON_LDADD)
libfile_la_LDFLAGS = $(COMMON_LDFLAGS)

libethernet_la_SOURCES = src/plugins/ethernet.c
libethernet_la_CFLAGS  = $(COMMON_CFLAGS)
libethernet_la_LIBADD  = $(COMMON_LDADD)
libethernet_la_LDFLAGS = $(COMMON_LDFLAGS)

librs232_la_SOURCES = src/plugins/rs232.c
librs232_la_CFLAGS  = $(COMMON_CFLAGS)
librs232_la_LIBADD  = $(COMMON_LDADD)
librs232_la_LDFLAGS = $(COMMON_LDFLAGS)

libpin_entry_la_SOURCES = src/plugins/pin_entry.c
libpin_entry_la_CFLAGS  = $(COMMON_CFLAGS)
libpin_entry_la_LIBADD  = $(COMMON_LDADD)
libpin_entry_la_LDFLAGS = $(COMMON_LDFLAGS)

libdatetime_la_SOURCES = src/plugins/datetime.c
libdatetime_la_CFLAGS  = $(COMMON_CFLAGS)
libdatetime_la_LIBADD  = $(COMMON_LDADD)
libdatetime_la_LDFLAGS = $(COMMON_LDFLAGS)

libkeypad_la_SOURCES = src/plugins/keypad.c
libkeypad_la_CFLAGS  = $(COMMON_CFLAGS)
libkeypad_la_LIBADD  = $(COMMON_LDADD)
libkeypad_la_LDFLAGS = $(COMMON_LDFLAGS)

libemv_contact_la_SOURCES = src/plugins/emv_contact.c
libemv_contact_la_CFLAGS  = $(COMMON_CFLAGS)
libemv_contact_la_LIBADD  = $(COMMON_LDADD)
libemv_contact_la_LDFLAGS = $(COMMON_LDFLAGS)

libemv_contactless_la_SOURCES = src/plugins/emv_contactless.c
libemv_contactless_la_CFLAGS  = $(COMMON_CFLAGS)
libemv_contactless_la_LIBADD  = $(COMMON_LDADD)
libemv_contactless_la_LDFLAGS = $(COMMON_LDFLAGS)

libbackend_la_SOURCES = src/plugins/backend.c
libbackend_la_CFLAGS  = $(COMMON_CFLAGS)
libbackend_la_LIBADD  = $(COMMON_LDADD)
libbackend_la_LDFLAGS = $(COMMON_LDFLAGS)


luna_SOURCES  = src/main.c                                                   \
                src/plugin.c                                                 \
                src/ssl_locks.c                                              \
                src/cli/parse_opts.c                                         \
                src/bindings/lua.c                                           \
                src/bindings/lua/ctos.c                                      \
                src/bindings/lua/device.c                                    \
                src/bindings/lua/logger.c                                    \
                src/bindings/lua/printer.c                                   \
                src/bindings/lua/settings.c                                  \
                src/bindings/lua/services.c                                  \
                src/bindings/lua/timer.c                                     \
                src/bindings/lua/tokenizer.c                                 \
                src/bindings/lua/xml.c                                       \
                src/bindings/lua/zmq.c                                       \
                src/services/bluetooth.c                                     \
                src/services/events_proxy.c                                  \
                src/services/input.c                                         \
                src/services/logger.c                                        \
                src/services/settings.c                                      \
                src/services/timer.c                                         \
                src/services/tokenizer.c                                     \
                src/services/touchscreen.c                                   \
                src/services/usb.c                                           \
                src/services/webserver.c                                     \
                src/services/wifi.c                                          \
                src/util/base64_helpers.c                                    \
                src/util/curl_utils.c                                        \
                src/util/detokenize_template.c                               \
                src/util/emv_helpers.c                                       \
                src/util/encryption_helpers.c                                \
                src/util/files.c                                             \
                src/util/headers_parser.c                                    \
                src/util/https_request.c                                     \
                src/util/jsmn.c                                              \
                src/util/jsmn_helpers.c                                      \
                src/util/lrc.c                                               \
                src/util/luhn.c                                              \
                src/util/machine_id.c                                        \
                src/util/md5_helpers.c                                       \
                src/util/migrator.c                                          \
                src/util/string_helpers.c                                    \
                src/util/tlv.c
luna_CFLAGS   = $(COMMON_CFLAGS)
luna_LDADD    = $(COMMON_LDADD)
luna_LDFLAGS  = $(COMMON_LDFLAGS)
