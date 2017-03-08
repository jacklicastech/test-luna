AC_DEFUN([APPILEE_DEVICE_CHECK], [

PKG_CHECK_MODULES([ZMQ],       [libzmq >= 4.1.4])
PKG_CHECK_MODULES([CZMQ],      [libczmq >= 3.0.2])
PKG_CHECK_MODULES([LUA],       [liblua >= 5.3.2])
PKG_CHECK_MODULES([XML2],      [libxml-2.0 >= 2.6.29])
PKG_CHECK_MODULES([URIPARSER], [liburiparser >= 0.8.4])
AC_CHECK_LIB([backtrace], [backtrace_create_state])

AC_ARG_VAR([TARGET_DEVICE],              [mp200, vega3000, mars1000, or none for native linux])
AC_ARG_VAR([DECRYPTION_KEY_PASSPHRASE],  [Passphrase to use to decode the private decryption key])
AC_ARG_VAR([SSL_CIPHER_LIST],            [List of OpenSSL ciphers to use])
AM_CONDITIONAL([PASSPHRASE_NOT_PRESENT], [test "x$DECRYPTION_KEY_PASSPHRASE" = "x"])
AM_COND_IF([PASSPHRASE_NOT_PRESENT],
           [AC_DEFINE_UNQUOTED([DECRYPTION_KEY_PASSPHRASE], ["v\x28Nek9YbO~!OnBM6"],        [Passphrase to use to decode the private decryption key])],
           [AC_DEFINE_UNQUOTED([DECRYPTION_KEY_PASSPHRASE], ["$DECRYPTION_KEY_PASSPHRASE"], [Passphrase to use to decode the private decryption key])])
AM_CONDITIONAL([USE_DEFAULT_SSL_CIPHER_LIST], [test "x$SSL_CIPHER_LIST" = "x"])
AM_COND_IF([USE_DEFAULT_SSL_CIPHER_LIST],
           [AC_DEFINE_UNQUOTED([LUNA_SSL_CIPHER_LIST],      ["EECDH+ECDSA+AESGCM EECDH+aRSA+AESGCM EECDH+ECDSA+SHA256 EECDH+aRSA+SHA256 EECDH+aRSA+RC4 !EDH+aRSA EECDH RC4 !aNULL !eNULL !LOW !3DES !MD5 !EXP !PSK !SRP !DSS !RC4"],                        [List of OpenSSL ciphers to use])],
           [AC_DEFINE_UNQUOTED([LUNA_SSL_CIPHER_LIST],      ["$SSL_CIPHER_LIST"],           [List of OpenSSL ciphers to use])])
#           [AC_DEFINE_UNQUOTED([LUNA_SSL_CIPHER_LIST],      ["HIGH:!aNULL:!MD5:!RC4"],                        [List of OpenSSL ciphers to use])],
AM_CONDITIONAL([SET_DEFAULT_LOG_LEVEL], [test "x$DEFAULT_LOG_LEVEL" = "x"])
AM_COND_IF([SET_DEFAULT_LOG_LEVEL],
           [AC_DEFINE_UNQUOTED([CAG_LOG_LEVEL], [LOG_LEVEL_DEBUG], [Default log level when app initially starts])],
           [AC_DEFINE_UNQUOTED([CAG_LOG_LEVEL], [LOG_LEVEL_$DEFAULT_LOG_LEVEL], [Default log level when app initially starts])])


# FIXME insecure path should be a subdirectory, not '.', in order to prevent
# scripts from accessing important files...
AC_DEFINE([INSECURE_PATH_PREFIX],              ["."],                [Path prefix for insecure files, e.g. referenced from scripts])
AC_DEFINE([LOG_INSECURE_MESSAGES],             [1],                  [Whether to allow logging of insecure messages])
AC_DEFINE([DEFAULT_USERNAME],                  ["apiuser"],          [The default admin username])
AC_DEFINE([DEFAULT_PASSWORD],                  ["1397139"],          [The default admin password])
AC_DEFINE([DEFAULT_AUTOUPDATE_S3_BUCKET_NAME], ["appilee"],          [The default S3 bucket to search for updates])
AC_DEFINE([DEFAULT_AUTOUPDATE_S3_ENDPOINT],    ["s3.amazonaws.com"], [The default S3 endpoint; this may need to change depending on region])
AC_DEFINE([DEFAULT_AUTOUPDATE_CHECK_INTERVAL], ["21600000"],         [The default frequency, in ms, to search for updates])
AC_DEFINE([DEFAULT_WEBSERVER_BEACON_PORT],     ["33310"],            [The default port to send UDP broadcasts to advertise the location of the webserver])
AC_DEFINE([DEFAULT_WEBSERVER_BEACON_ENABLED],  ["true"],             [Whether UDP broadcasting is enabled by default])
AC_DEFINE([DEFAULT_WEBSERVER_PORT],            [44443],              [The default port to listen for HTTPS requests])
AC_DEFINE([MULTI_PATH_SEPARATOR],              [':'],                [The character that separates multiple directories in PATH])
AC_DEFINE([FILE_PATH_SEPARATOR],               ['/'],                [The character that separates directories in a file system path])
case "$TARGET_DEVICE" in
  mp200)
    AC_DEFINE_UNQUOTED([DEFAULT_READ_PATHS],  ["./fs_data"],                        [The paths from which files may be read; can be overridden at runtime with READ_PATHS])
    AC_DEFINE_UNQUOTED([DEFAULT_WRITE_PATHS], ["./fs_data"],                        [The path to which files must be written; can be overridden at runtime with WRITE_PATH])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_PATH],    ["./fs_data/?.lua;;"],                [The default search path for lua libraries; if LUA_PATH is set at runtime, it will be searched first])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_CPATH],   ["./fs_data/bindings/liblua_?.so;;"], [The default search path for lua C extensions; if LUA_CPATH is set at runtime, it will be searched first])
    AC_DEFINE([DEFAULT_DEVICE_NAME], ["mp200"], [The default name of this device])
    AC_DEFINE([HAVE_CTOS], [1], [Use CTOS])
    AC_SUBST([DEVICE_CFLAGS], [" \
      -mcpu=arm1136j-s -msoft-float -fomit-frame-pointer                     \
      -fno-dollars-in-identifiers -fsigned-char -I${LO_SDK_TOP_DIR}/include"])
    AC_CHECK_LIB([ctosapi], [CTOS_LCDGCaptureWindow], [AC_DEFINE([HAVE_CTOS_CAPTURE_WINDOW], [1], [CTOS window capture function])], [], [
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcaclvw -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite -lcaemvl2 -lcaaep -lcaclentry -lcaclmdl -lcacqp -lcaifh -lcajct
      -lcampp -lcavap -lcavpw
    ])
    AC_CHECK_LIB([ctosapi], [CTOS_LCDGShowPicEx], [AC_DEFINE([HAVE_CTOS_SHOW_PIC_EX], [1], [CTOS show bytes as image function])], [], [
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcaclvw -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite -lcaemvl2 -lcaaep -lcaclentry -lcaclmdl -lcacqp -lcaifh -lcajct
      -lcampp -lcavap -lcavpw
    ])
    AC_LIB_HAVE_LINKFLAGS([ctosapi], [
      caethernet cafs capmodem camodem curl ssl crypto cagsm cauldpm cauart
      calcd cafont freetype cartc cakms cabarcode caprt catls causbh
      bluetooth casqlite
    ])
    # contact EMV
    AC_LIB_HAVE_LINKFLAGS([caemvl2], [ctosapi])
    # contactless EMV
    AC_CHECK_LIB([caclentry], [EMVCL_CompleteEx], [AC_DEFINE([HAVE_EMVCL], [1], [CTOS contactless])], [], [
      -lcaaep -lcaclmdl -lcacqp -lcaifh -lcajct -lcampp -lcavap -lcavpw -lcaclvw -lctosapi
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite
    ])
    AC_LIB_HAVE_LINKFLAGS([caclentry], [caaep caclmdl cacqp caifh cajct campp cavap cavpw caclvw ctosapi])
    AC_CHECK_LIB([caethernet], [CTOS_EthernetOpenEx], [AC_DEFINE([HAVE_ETHERNET], [1], [CTOS ethernet])], [], [
      -lctosapi -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite
    ])
    ;;
  mars1000)
    AC_DEFINE_UNQUOTED([DEFAULT_READ_PATHS],  ["./fs_data"],                        [The paths from which files may be read; can be overridden at runtime with READ_PATHS])
    AC_DEFINE_UNQUOTED([DEFAULT_WRITE_PATHS], ["./fs_data"],                        [The path to which files must be written; can be overridden at runtime with WRITE_PATH])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_PATH],    ["./fs_data/?.lua;;"],                [The default search path for lua libraries; if LUA_PATH is set at runtime, it will be searched first])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_CPATH],   ["./fs_data/bindings/liblua_?.so;;"], [The default search path for lua C extensions; if LUA_CPATH is set at runtime, it will be searched first])
    AC_DEFINE([DEFAULT_DEVICE_NAME], ["mars1000"], [The default name of this device])
    AC_DEFINE([HAVE_CTOS], [1], [Use CTOS])
    AC_SUBST([DEVICE_CFLAGS], [" \
      -mcpu=arm1136j-s -msoft-float -fomit-frame-pointer                     \
      -fno-dollars-in-identifiers -fsigned-char -I${LO_SDK_TOP_DIR}/include"])
    AC_CHECK_LIB([ctosapi], [CTOS_LCDGCaptureWindow], [AC_DEFINE([HAVE_CTOS_CAPTURE_WINDOW], [1], [CTOS window capture function])], [], [
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcabarcode -lcaprt -lcatls -lcausbh
      -lcasqlite
    ])
    AC_CHECK_LIB([ctosapi], [CTOS_LCDGShowPicEx], [AC_DEFINE([HAVE_CTOS_SHOW_PIC_EX], [1], [CTOS show bytes as image function])], [], [
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcabarcode -lcaprt -lcatls -lcausbh
      -lcasqlite
    ])
    AC_LIB_HAVE_LINKFLAGS([ctosapi], [
      caethernet cafs capmodem camodem curl ssl crypto cagsm cauldpm cauart
      calcd cafont freetype cartc cakms cabarcode caprt catls causbh
      casqlite
    ])
    # contact EMV
    AC_LIB_HAVE_LINKFLAGS([caemvl2], [ctosapi caemvl2ap])
    # contactless EMV
    AC_CHECK_LIB([caclentry], [EMVCL_CompleteEx], [AC_DEFINE([HAVE_EMVCL], [1], [CTOS contactless])], [], [
      -lcaclmdl -lcampp -lcavpw -lcaclvw -lctosapi -lcrypto -lrt
    ])
    AC_LIB_HAVE_LINKFLAGS([caclentry], [caclmdl campp cavpw caclvw ctosapi crypto rt])
    AC_CHECK_LIB([caethernet], [CTOS_EthernetOpenEx], [AC_DEFINE([HAVE_ETHERNET], [1], [CTOS ethernet])], [], [
      -lctosapi -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcabarcode -lcaprt -lcatls -lcausbh
      -lcasqlite
    ])
    ;;
  vega3000)
    AC_DEFINE_UNQUOTED([DEFAULT_READ_PATHS],  ["./fs_data"],                        [The paths from which files may be read; can be overridden at runtime with READ_PATHS])
    AC_DEFINE_UNQUOTED([DEFAULT_WRITE_PATHS], ["./fs_data"],                        [The path to which files must be written; can be overridden at runtime with WRITE_PATH])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_PATH],    ["./fs_data/?.lua;;"],                [The default search path for lua libraries; if LUA_PATH is set at runtime, it will be searched first])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_CPATH],   ["./fs_data/bindings/liblua_?.so;;"], [The default search path for lua C extensions; if LUA_CPATH is set at runtime, it will be searched first])
    AC_DEFINE([DEFAULT_DEVICE_NAME], ["vega3000"], [The default name of this device])
    AC_DEFINE([HAVE_CTOS], [1], [Use CTOS])
    AC_SUBST([DEVICE_CFLAGS], ["-fsigned-char -I${LO_SDK_TOP_DIR}/include"])
    AC_CHECK_LIB([ctosapi], [CTOS_LCDGCaptureWindow], [AC_DEFINE([HAVE_CTOS_CAPTURE_WINDOW], [1], [CTOS window capture function])], [], [
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcaclvw -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite -lcaemvl2 -lcaaep -lcaclentry -lcaclmdl -lcacqp -lcaifh -lcajct
      -lcampp -lcavap -lcavpw
    ])
    AC_CHECK_LIB([ctosapi], [CTOS_LCDGShowPicEx], [AC_DEFINE([HAVE_CTOS_SHOW_PIC_EX], [1], [CTOS show bytes as image function])], [], [
      -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcaclvw -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite -lcaemvl2 -lcaaep -lcaclentry -lcaclmdl -lcacqp -lcaifh -lcajct
      -lcampp -lcavap -lcavpw
    ])
    # contact EMV
    AC_LIB_HAVE_LINKFLAGS([caemvl2], [ctosapi caemvl2ap
      caethernet cafs capmodem camodem curl ssl crypto
      cagsm cauldpm cauart calcd cafont freetype cartc cakms
      cabarcode caprt catls causbh casqlite rt
    ])
    # contactless EMV
    AC_LIB_HAVE_LINKFLAGS([ctosapi], [
      caethernet cafont cafs cakms calcd camodem capmodem caprt cartc cauart
      cauldpm causbh cagsm cabarcode pthread dl caclvw catls crypto curl ssl
      z freetype casqlite caemvl2
    ])
    AC_CHECK_LIB([caclentry], [EMVCL_CompleteEx], [AC_DEFINE([HAVE_EMVCL], [1], [CTOS contactless])], [], [
      -lcaclmdl -lcampp -lcavpw -lcaclvw

      -lctosapi -lcaethernet -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto
      -lcagsm -lcauldpm -lcauart -lcalcd -lcafont -lfreetype -lcartc -lcakms
      -lcabarcode -lcaprt -lcatls -lcausbh -lcasqlite -lrt
    ])
    AC_LIB_HAVE_LINKFLAGS([caclentry], [
      caclmdl campp cavpw caclvw ctosapi crypto rt cartc cauart calcd
      caethernet cakms cafont cafs curl freetype ssl
    ])
    AC_CHECK_LIB([caethernet], [CTOS_EthernetOpenEx], [AC_DEFINE([HAVE_ETHERNET], [1], [CTOS ethernet])], [], [
      -lctosapi -lcafs -lcapmodem -lcamodem -lcurl -lssl -lcrypto -lcagsm -lcauldpm -lcauart
      -lcalcd -lcafont -lfreetype -lcartc -lcakms -lcabarcode -lcaprt -lcatls -lcausbh
      -lbluetooth -lcasqlite
    ])
    ;;
  *)
    AC_DEFINE_UNQUOTED([DEFAULT_READ_PATHS],  [".:./tmp:/var/lib/${PACKAGE_NAME}"],                 [The paths from which files may be read; can be overridden at runtime with READ_PATHS])
    AC_DEFINE_UNQUOTED([DEFAULT_WRITE_PATHS], ["./tmp"],                                            [The path to which files must be written; can be overridden at runtime with WRITE_PATH])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_PATH],    ["/var/lib/${PACKAGE_NAME}/lib/?.lua;./?.lua;;"],     [The default search path for lua libraries; if LUA_PATH is set at runtime, it will be searched first])
    AC_DEFINE_UNQUOTED([DEFAULT_LUA_CPATH],   ["/var/lib/${PACKAGE_NAME}/bindings/liblua_?.so;;"],  [The default search path for lua C extensions; if LUA_CPATH is set at runtime, it will be searched first])
    AC_DEFINE_UNQUOTED([DEFAULT_DEVICE_NAME], ["debian-amd64"],                                     [The default name of this device])
    AC_DEFINE_UNQUOTED([DEBIAN],              [1],                                                  [Define if building for Debian Linux])
    PKG_CHECK_MODULES([DBUS],      [dbus-1    >= 1.6.18], [AC_DEFINE([HAVE_DBUS], [1], [Use DBUS])])
    PKG_CHECK_MODULES([CURL],      [libcurl   >= 7.35.0])
    PKG_CHECK_MODULES([LIBSSL],    [libssl    >= 1.0.1f])
    PKG_CHECK_MODULES([LIBCRYPTO], [libcrypto >= 1.0.1f])
    PKG_CHECK_MODULES([SQLITE3],   [sqlite3   >= 3.5])
    AC_DEFINE([HAVE_ETHERNET], [1], [Ethernet])
    ;;
esac

# FIXME don't reference /tip in production, only in development builds
AC_DEFINE([DEFAULT_AUTOUPDATE_S3_PREFIX], [PACKAGE_TARNAME "/tip/" DEFAULT_DEVICE_NAME], [The default S3 prefix which denotes updates compatible with this device])
AC_DEFINE([TOKEN_PREFIX], ["{{" PACKAGE_TARNAME "_token_"], [The prefix which surrounds a token ID in template strings])
AC_DEFINE([TOKEN_SUFFIX], ["}}"],                        [The suffix which surrounds a token ID in template strings])
AM_CONDITIONAL([HAVE_CTOS], [test x$HAVE_LIBCTOSAPI = xyes])

])