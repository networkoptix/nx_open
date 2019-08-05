FROM ubuntu:18.04
MAINTAINER "Dmitry Kargin" <dkargin@networkoptix.com>

# This Dockerfile was based on the knowledge from the following sources:
#  - some words about systemd problems inside docker https://developers.redhat.com/blog/2014/05/05/running-systemd-within-docker-container/ 
#  - some parts from https://github.com/defn/docker-systemd

# This string is necessary for systemd to know we are working inside container environment
ENV container docker

# This will override our EULA check. 
ENV DEBIAN_FRONTEND noninteractive

# This is the deb file used to install mediaserver
ARG mediaserver_deb

RUN ["/bin/bash", "-c", ": ${mediaserver_deb:?Missing source deb file. You should set --build-arg mediaserver_deb=path_to_mediaserver.deb!}"]

COPY manage.sh /setup/
RUN chmod a+x /setup/manage.sh

# We need:
#  - bin-utils for patching cloud host
#  - lsb-release is used as a part of install scripts inside our deb package
RUN apt-get update && apt-get install -y dbus systemd lsb-release binutils && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN find /etc/systemd -name '*.timer' | xargs rm -v

RUN systemctl set-default multi-user.target

STOPSIGNAL SIGRTMIN+3

# Using systemd as an entry point. It will launch mediaserver according to systemd scripts.
ENTRYPOINT ["/sbin/init", "--log-target=journal"]

COPY "${mediaserver_deb}" /setup/

# Installing the mediaserver. It will download and install requirements as well.
RUN apt-get update -qq && apt install -y -qq --no-install-recommends "/setup/${mediaserver_deb}" && mkdir -p /opt/networkoptix/mediaserver/var

# Override for cloud host.
# We keep this argumet last to prevent docker rebuilding all previous steps.
ARG cloud_host
RUN /setup/manage.sh --cloud "$cloud_host" --norestart
