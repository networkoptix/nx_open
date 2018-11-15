FROM ubuntu:16.04

# no tty
ENV DEBIAN_FRONTEND noninteractive

COPY docker/01proxy /etc/apt/apt.conf.d/01proxy
COPY docker/sources.list.d /etc/apt/sources.list.d/
COPY docker/apt_keys /tmp/apt_keys/
COPY docker/get-pip.py /
COPY docker/libmysqlclient18_5.6.25-0ubuntu1_amd64.deb /tmp/

RUN dpkg-divert --local --rename --add /sbin/initctl \
        && ln -sf /bin/true /sbin/initctl \
        && for f in /tmp/apt_keys/*; do apt-key add $f; done \
        && rm -rf /tmp/apt_keys \
        && apt-get update \
        && apt-get install -y --no-install-recommends software-properties-common \
        && add-apt-repository ppa:deadsnakes/ppa \
        && apt-get update \
        && apt-get install -y --no-install-recommends wget psmisc \
            libsm6 libice6 libaudio2 libogg0 libxfixes3 libxrender1 libfontconfig1 libgl1-mesa-glx libglib2.0-0 \
            dnsutils mysql-community-client strace gdb ca-certificates \
            libpython2.7 libpython3.6 \
            libmysqlclient20 openssh-client apache2-utils libltdl7 gettext-base \
            python python3.6 nginx-extras libncurses5 \
            libcurl3 libstdc++6 locales \
            inetutils-ping net-tools vim-nox \
        && dpkg -i /tmp/libmysqlclient18_5.6.25-0ubuntu1_amd64.deb \
        && rm /tmp/libmysqlclient18_5.6.25-0ubuntu1_amd64.deb \
        && locale-gen en_US.UTF-8 \
        && update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.6 1 \
        && /usr/bin/wget -O /usr/local/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.0.2/dumb-init_1.0.2_amd64 \
        && chmod +x /usr/local/bin/dumb-init \
        && rm -rf /var/lib/apt/lists/* \
        && python2 get-pip.py \
        && python3.6 get-pip.py \
        && rm /get-pip.py \
        && pip2 install pyOpenSSL==17.5.0

COPY docker/config_helper.py /usr/local/bin/

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
