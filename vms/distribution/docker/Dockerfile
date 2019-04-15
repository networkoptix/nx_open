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

RUN apt-get update && apt-get install -y dbus systemd lsb-release && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN find /etc/systemd -name '*.timer' | xargs rm -v

RUN systemctl set-default multi-user.target

STOPSIGNAL SIGRTMIN+3

# Using systemd as an entry point. It will launch mediaserver according to systemd scripts.
ENTRYPOINT ["/sbin/init", "--log-target=journal"]

# This is strange workaround for systemd stdout problem. Not sure what is going on, actually.
CMD []

RUN mkdir -p /debs

COPY "${mediaserver_deb}" /debs/

# Installing the mediaserver
RUN apt-get update -qq && apt install -y -qq --no-install-recommends "/debs/${mediaserver_deb}" && mkdir -p /opt/networkoptix/mediaserver/var
