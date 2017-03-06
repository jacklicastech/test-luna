# build castles-base at ../base
FROM castles-base:latest

MAINTAINER Colin MacKenzie IV <colin@appilee.com>

ADD sdk.tgz /opt/castles

# e.g. MP200, V3
ARG SDK=MP200

# e.g. mp200, vega3000
ARG TARGET_DEVICE=mp200

ENV LO_SDK_TOP_DIR=/opt/castles/SDK_$SDK
ENV LO_SDK_LIB_DIR=$LO_SDK_TOP_DIR/lib
ENV LO_SDK_INCLUDE_DIR=$LO_SDK_TOP_DIR/include
ENV LO_SDK_AP_LIB_DIR=$LO_SDK_TOP_DIR/rootfs/AP_SO_LIB
ENV TOOLCHAIN_BINDIR=/opt/castles/toolchain/arm_toolchain_32bit/tools/le_arm11_external_20090506_32bit/bin
ENV TOOLCHAIN_SYSROOT=/opt/castles/toolchain/arm_toolchain_32bit/tools/le_arm11_external_20090506_32bit/sysroot
ENV TOOLCHAIN_SYSROOT_LIB=$TOOLCHAIN_SYSROOT/usr/lib
ENV AS=arm-brcm-linux-gnueabi-as
ENV LDFLAGS="-Wl,-rpath -Wl,/usr/lib -Wl,-rpath-link -Wl,$LO_SDK_LIB_DIR -L$LO_SDK_LIB_DIR -L/lib"
ENV AR=arm-brcm-linux-gnueabi-ar
ENV SDK_TYPE=SDK_$SDK
ENV CLFS_HOST=x86_64-cross-linux-gnu
ENV NM=arm-brcm-linux-gnueabi-nm
ENV CLFS_TARGET=arm-linux
ENV TARGET_DEVICE=$TARGET_DEVICE
ENV LO_SDK_ROOTFS_DIR=$LO_SDK_TOP_DIR/rootfs
ENV LO_SDK_DRIVER_DIR=$LO_SDK_ROOTFS_DIR/Driver_ko
ENV TOOLCHAIN_SDKDIR=/opt/castles/toolchain/589x_SDK_44
ENV SDK_SRC_BUILD_DIR=/opt/castles/build
ENV CPP=arm-brcm-linux-gnueabi-cpp 
ENV PATH=$TOOLCHAIN_BINDIR:/opt/castles/build:$PATH
ENV MODEL=$TARGET_DEVICE
ENV LD=arm-brcm-linux-gnueabi-ld 
ENV AR2=arm-brcm-linux-gnueabi-ranlib
ENV STRIP=arm-brcm-linux-gnueabi-strip
ENV PKG_CONFIG_LIBDIR=$TOOLCHAIN_BINDIR/../sysroot/usr/lib/pkgconfig
ENV CXX=arm-brcm-linux-gnueabi-g++ 
ENV TOOLCHAIN_SUFFIX=_external_20090506_32bit
ENV TOOLCHAIN_DIR=/opt/castles/toolchain/arm_toolchain_32bit/tools
ENV PKG_CONFIG_PATH=$LO_SDK_LIB_DIR/pkgconfig
ENV RANLIB=arm-brcm-linux-gnueabi-ranlib
ENV ARCH=arm
ENV CC=arm-brcm-linux-gnueabi-gcc 
ENV CROSS_COMPILE=arm-brcm-linux-gnueabi-
ENV PACKAGES_ROOT_DIR=/opt/castles/toolchain/arm_toolchain_32bit/packages/le_arm11_external_20090506_32bit
ENV LIBSODIUM_VERSION=1.0.11
ENV LIBZMQ_VERSION=4.1.5
ENV LIBCZMQ_VERSION=3.0.2
ENV LIBURIPARSER_VERSION=0.8.4
ENV LIBLUA_VERSION=5.3.2

RUN mkdir -p /tmp/src

RUN wget -O /tmp/src/libsodium-$LIBSODIUM_VERSION.tar.gz https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz
RUN cd /tmp/src                                                           && \
    tar xzf libsodium-$LIBSODIUM_VERSION.tar.gz                           && \
    cd libsodium-$LIBSODIUM_VERSION                                       && \
    ./configure --host=arm-brcm-linux-gnueabi --prefix=$LO_SDK_TOP_DIR       \
                LIBS=-L$LO_SDK_LIB_DIR                                    && \
    make && make install || (cat config.log && exit 1)

RUN wget -O /tmp/src/zeromq-$LIBZMQ_VERSION.tar.gz https://github.com/zeromq/zeromq4-1/releases/download/v$LIBZMQ_VERSION/zeromq-$LIBZMQ_VERSION.tar.gz
RUN cd /tmp/src                                                           && \
    tar xzf zeromq-$LIBZMQ_VERSION.tar.gz                                 && \
    cd zeromq-$LIBZMQ_VERSION                                             && \
    ./configure --host=arm-brcm-linux-gnueabi --prefix=$LO_SDK_TOP_DIR       \
                LIBS=-L$LO_SDK_LIB_DIR --with-relaxed                 && \
    make && make install || (cat config.log && exit 1)

# czmq has UTF-8 BOM's in the source files, but our gcc doesn't recognize them
# so we have to strip them before building.
RUN wget -O /tmp/src/czmq-$LIBCZMQ_VERSION.tar.gz       https://github.com/zeromq/czmq/releases/download/v$LIBCZMQ_VERSION/czmq-$LIBCZMQ_VERSION.tar.gz
RUN cd /tmp/src                                                           && \
    tar xzf czmq-$LIBCZMQ_VERSION.tar.gz                                  && \
    cd czmq-$LIBCZMQ_VERSION                                              && \
    find . -name '*.c' -exec sed -i '1 s/^\xef\xbb\xbf//' {} \;           && \
    find . -name '*.h' -exec sed -i '1 s/^\xef\xbb\xbf//' {} \;           && \
    ./configure --host=arm-brcm-linux-gnueabi --prefix=$LO_SDK_TOP_DIR       \
                LIBS=-L$LO_SDK_LIB_DIR                                && \
    make && make install || (cat config.log && exit 1)

RUN wget -O /tmp/src/uriparser-$LIBURIPARSER_VERSION.tar.bz2 http://downloads.sourceforge.net/project/uriparser/Sources/$LIBURIPARSER_VERSION/uriparser-$LIBURIPARSER_VERSION.tar.bz2
RUN cd /tmp/src                                                           && \
    bunzip2 uriparser-$LIBURIPARSER_VERSION.tar.bz2                       && \
    tar xvf uriparser-$LIBURIPARSER_VERSION.tar                           && \
    cd uriparser-$LIBURIPARSER_VERSION                                    && \
    ./configure --host=arm-brcm-linux-gnueabi --prefix=$LO_SDK_TOP_DIR       \
                LIBS=-L$LO_SDK_LIB_DIR --disable-test --disable-doc   && \
    make && make install || (cat config.log && exit 1)

RUN wget -O /tmp/src/lua-$LIBLUA_VERSION.tar.gz        https://www.lua.org/ftp/lua-$LIBLUA_VERSION.tar.gz
RUN cd /tmp/src                                                           && \
    tar xzf lua-$LIBLUA_VERSION.tar.gz                                    && \
    cd lua-$LIBLUA_VERSION/src                                            && \
    sed -i 's/\/\/#define LUA_32BITS/#define LUA_32BITS/g'   luaconf.h    && \
    sed -i 's/\/\/#define LUA_USE_C89/#define LUA_USE_C89/g' luaconf.h    && \
    sed -i 's/\/\* #define LUA_32BITS \*\//#define LUA_32BITS/g'   luaconf.h && \
    sed -i 's/\/\* #define LUA_USE_C89 \*\//#define LUA_USE_C89/g' luaconf.h && \
    $CC -Wl,-soname,liblua.so.5 -fPIC -shared -std=gnu99 -O2 -Wall -Wextra   \
        -DLUA_COMPAT_5_2 -DLUA_USE_LINUX -lm -Wl,-E -ldl -lreadline          \
        -o liblua.so.$LIBLUA_VERSION                                         \
        `find . ! -name "lua.c" ! -name "luac.c" -name "*.c"`             && \
    cp liblua.so.$LIBLUA_VERSION $LO_SDK_LIB_DIR                          && \
    cp lua.h lauxlib.h luaconf.h lualib.h $LO_SDK_INCLUDE_DIR             && \
    cd $LO_SDK_LIB_DIR                                                    && \
    ln -s liblua.so.$LIBLUA_VERSION liblua.so

ADD pkgconfig/liblua.pc $LO_SDK_LIB_DIR/pkgconfig/liblua.pc

ADD luna.tgz /tmp/src

RUN cd /tmp/src/luna                                                      && \
    autoreconf -vi && \
    ./configure --host=arm-brcm-linux-gnueabi --prefix=$LO_SDK_TOP_DIR       \
                LIBS=-L$LO_SDK_TOP_DIR/lib                                && \
    ./makeso

# these redundant directory names are to keep capgen from segfaulting...
RUN cd /tmp/src/luna                                                      && \
    mkdir -p dist/deps/libsodium                                             \
             dist/deps/libzmq                                                \
             dist/deps/libczmq                                               \
             dist/deps/liburiparser                                          \
             dist/deps/libreadline                                           \
             dist/deps/liblua                                                \
             dist/bin/bin                                                    \
             dist/output                                                  && \
    cp -L $LO_SDK_LIB_DIR/libsodium.so          dist/deps/libsodium       && \
    cp -L $LO_SDK_LIB_DIR/libzmq.so             dist/deps/libzmq          && \
    cp -L $LO_SDK_LIB_DIR/libczmq.so            dist/deps/libczmq         && \
    cp -L $LO_SDK_LIB_DIR/liburiparser.so       dist/deps/liburiparser    && \
    cp -L $LO_SDK_LIB_DIR/liblua.so             dist/deps/liblua          && \
    cp -L $TOOLCHAIN_SYSROOT_LIB/libreadline.so dist/deps/libreadline     && \
    cp -L -r ./resources                        dist/resources            && \
    mv /tmp/src/luna/dist/bin/luna /tmp/src/luna/dist/bin/bin/luna        && \
    tar czf luna.tar.gz dist

RUN cd /tmp/src/luna/dist &&                                                 \
    capgen --input=program --build=0000 --company=Appilee --name=luna        \
           --target=$TARGET_DEVICE bin/bin/luna                           && \
    mv bin/output/bin.CAP ./output/application.cap

RUN cd /tmp/src/luna/dist &&                                                 \
    capgen --input=assets --build=0000 --company=Appilee --name=luna         \
           --target=$TARGET_DEVICE plugins/plugins                        && \
    mv plugins/output/plugins.CAP ./output/plugins.cap

RUN cd /tmp/src/luna/dist &&                                                 \
    capgen --input=assets --build=0000 --company=Appilee --name=luna         \
           --target=$TARGET_DEVICE bindings/bindings                      && \
    mv bindings/output/bindings.CAP ./output/bindings.cap

RUN cd /tmp/src/luna/dist &&                                                 \
    mkdir -p ./resources                                                  && \
    cp -r ../resources ./resources/resources                              && \
    capgen --input=assets --build=0000 --company=Appilee --name=luna         \
           --target=$TARGET_DEVICE resources/resources                    && \
    mv resources/output/resources.CAP ./output/resources.cap

RUN cd /tmp/src/luna/dist                                                 && \
    capgen --input=library --build=0000 --company=Appilee --name=luna        \
           --target=$TARGET_DEVICE deps/libsodium/libsodium.so            && \
    mv deps/output/libsodium.CAP ./output/libsodium.cap

RUN cd /tmp/src/luna/dist                                                 && \
    capgen --input=library --build=0000 --company=Appilee --name=luna        \
           --target=$TARGET_DEVICE deps/libzmq/libzmq.so                  && \
    mv deps/output/libzmq.CAP ./output/libzmq.cap

RUN cd /tmp/src/luna/dist                                                 && \
    capgen --input=library --build=0000 --company=Appilee --name=luna        \
           --target=$TARGET_DEVICE deps/libczmq/libczmq.so                && \
    mv deps/output/libczmq.CAP ./output/libczmq.cap

RUN cd /tmp/src/luna/dist                                                 && \
    capgen --input=library --build=0000 --company=Appilee --name=luna        \
           --target=$TARGET_DEVICE deps/liburiparser/liburiparser.so      && \
    mv deps/output/liburiparser.CAP ./output/liburiparser.cap

RUN cd /tmp/src/luna/dist                                                 && \
    capgen --input=library --build=0000 --company=Appilee --name=luna        \
           --target=$TARGET_DEVICE deps/liblua/liblua.so                  && \
    mv deps/output/liblua.CAP ./output/liblua.cap

# show list of build artifacts
RUN find /tmp/src/luna/dist -iname '*'
