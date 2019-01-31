FROM ubuntu:16.04

# no tty
ENV DEBIAN_FRONTEND noninteractive

RUN dpkg-divert --local --rename --add /sbin/initctl \
        && ln -sf /bin/true /sbin/initctl \
        && apt-get update \
        && apt-get install -y --no-install-recommends software-properties-common \
        && apt-get install -y --no-install-recommends wget psmisc \
            libsm6 libice6 libaudio2 libogg0 libxfixes3 libxrender1 libfontconfig1 libgl1-mesa-glx libglib2.0-0 \
            dnsutils strace gdb ca-certificates \
            libpython2.7 \
            libmysqlclient20 openssh-client apache2-utils libltdl7 gettext-base \
            python nginx-extras libncurses5 \
            inetutils-ping net-tools vim-nox \
        && /usr/bin/wget -O /usr/local/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.0.2/dumb-init_1.0.2_amd64 \
        && chmod +x /usr/local/bin/dumb-init \
        && mkdir /dock \
        && chown -R $USER: /dock \
        && chmod -R 777 /dock

# Set the locale
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

ARG VERSION
ARG REVISION
ARG BUILD_DATE
ARG BUILD_HOST
ARG BUILD_USER
LABEL version="$VERSION" \
      revision="$REVISION" \
      build-date="$BUILD_DATE" \
      build-host="$BUILD_HOST" \
      build-user="$BUILD_USER"

COPY stage/mediaserver /opt/networkoptix/mediaserver

COPY entrypoint.sh /opt/networkoptix/mediaserver/bin
COPY build/etc /opt/networkoptix/mediaserver/etc/
COPY stage/var /opt/networkoptix/mediaserver/var/

ENV QT_PLUGIN_PATH /opt/networkoptix/mediaserver/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/mediaserver/lib

CMD ["/opt/networkoptix/mediaserver/bin/entrypoint.sh"]
