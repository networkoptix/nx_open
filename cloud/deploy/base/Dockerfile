FROM golang:1.11.5-stretch as builder
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update \
        && apt-get install -y --no-install-recommends git \
        && go get -u github.com/vigasin/config_helper

FROM ubuntu:18.04

COPY --from=builder /go/bin/config_helper /usr/local/bin/

# no tty
ENV DEBIAN_FRONTEND noninteractive

COPY docker/01proxy /etc/apt/apt.conf.d/01proxy
COPY docker/sources.list.d /tmp/sources.list.d/
COPY docker/libmysqlclient18_5.6.25-0ubuntu1_amd64.deb /tmp/

RUN dpkg-divert --local --rename --add /sbin/initctl \
        && ln -sf /bin/true /sbin/initctl \
        && apt-get update \
        && apt-get install -y --no-install-recommends gnupg2 \
        && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 5072E1F5 \
        && mv /tmp/sources.list.d /etc/apt/sources.list.d \
        && apt-get update \
        && apt-get install gnupg2 \
        && apt-get upgrade -y \
        && apt-get install -y --no-install-recommends wget psmisc \
            libsm6 libice6 libaudio2 libogg0 libxfixes3 libxrender1 libfontconfig1 libgl1-mesa-glx libglib2.0-0 \
            dnsutils mysql-client strace ca-certificates \
            libmysqlclient20  apache2-utils libltdl7 gettext-base \
            libncurses5 libcurl3 libstdc++6 locales \
            busybox \
        && busybox --install \
        && dpkg -i /tmp/libmysqlclient18_5.6.25-0ubuntu1_amd64.deb \
        && rm /tmp/libmysqlclient18_5.6.25-0ubuntu1_amd64.deb \
        && locale-gen en_US.UTF-8 \
        && /usr/bin/wget -O /usr/local/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.0.2/dumb-init_1.0.2_amd64 \
        && chmod +x /usr/local/bin/dumb-init \
        && rm -rf /var/lib/apt/lists

# Set the locale
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

ARG VERSION
ARG REVISION
ARG BUILD_DATE
ARG BUILD_HOST
ARG BUILD_USER
LABEL maintainer="Ivan Vigasin <ivigasin@gmail.com>" \
      version="$VERSION" \
      revision="$REVISION" \
      build-date="$BUILD_DATE" \
      build-host="$BUILD_HOST" \
      build-user="$BUILD_USER"
