FROM ubuntu:14.04

MAINTAINER Colin MacKenzie IV <colin@appilee.com>

ADD toolchain.tgz /opt/castles
ADD capgen.tgz    /opt/castles

ENV LD_LIBRARY_PATH=/opt/castles/capgen

RUN apt-get update
RUN apt-get install -y wget build-essential git                              \
                       libxml2-dev mime-support automake libtool libc6-i386  \
                       autoconf automake m4 libtool  libsasl2-dev            \
                       libevent-dev perl pkg-config libcurl4-gnutls-dev      \
                       gettext curl libreadline-dev lib32stdc++6 p7zip-full  \
                       valgrind unzip

RUN apt-get install -y python

ENV PATH=$PATH:/opt/castles/capgen/

RUN cd /tmp                                                               && \
    curl "https://s3.amazonaws.com/aws-cli/awscli-bundle.zip"                \
         -o "awscli-bundle.zip"                                           && \
    unzip awscli-bundle.zip                                               && \
    ./awscli-bundle/install -i /usr/local/aws -b /usr/local/bin/aws
