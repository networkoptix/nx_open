FROM ubuntu:14.04
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

# keep upstart quiet
RUN dpkg-divert --local --rename --add /sbin/initctl
RUN ln -sf /bin/true /sbin/initctl

# no tty
ENV DEBIAN_FRONTEND noninteractive

COPY docker/01proxy /etc/apt/apt.conf.d/01proxy
COPY docker/mysql.list /etc/apt/sources.list.d/mysql.list
COPY docker/mysql_pubkey.txt /tmp/mysql_pubkey.txt

RUN apt-key add /tmp/mysql_pubkey.txt \
        && rm /tmp/mysql_pubkey.txt \
        && apt-get update && apt-get install -y --no-install-recommends wget psmisc \
            libsm6 libice6 libaudio2 libogg0 libxfixes3 libxrender1 libfontconfig1 libgl1-mesa-glx libglib2.0-0 \
            dnsutils mysql-community-client strace gdb software-properties-common \
            libmysqlclient18 libmysqlclient20 openssh-client apache2-utils libltdl7 gettext-base \
        && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
        && apt-get update \
        && apt-get install -y libstdc++6 \
        && /usr/bin/wget -O /usr/local/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.0.2/dumb-init_1.0.2_amd64 \
        && chmod +x /usr/local/bin/dumb-init \
        && rm -rf /var/lib/apt/lists/*

ADD https://github.com/kelseyhightower/confd/releases/download/v0.13.0/confd-0.13.0-linux-amd64 /usr/local/bin/confd
RUN chmod +x /usr/local/bin/confd

# Set the locale
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8
