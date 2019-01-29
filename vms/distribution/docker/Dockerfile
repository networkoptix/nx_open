FROM ubuntu:16.04
MAINTAINER "Dmitry Kargin" <dkargin@networkoptix.com>

# This Dockerfile was based on the knowledge from the following sources:
#  - some words about systemd problems inside docker https://developers.redhat.com/blog/2014/05/05/running-systemd-within-docker-container/ 
#  - some parts from https://github.com/defn/docker-systemd

# This string is necessary for systemd to know we are working inside container environment
ENV container docker

# This will override our EULA check. 
ENV DEBIAN_FRONTEND noninteractive

# This is the deb file used to install mediaserver
ARG mediaserver_deb=nxwitness-server-4.0.0.28230-linux64-beta-test.deb

RUN apt-get update && apt-get install -y dbus systemd && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN find /etc/systemd -name '*.timer' | xargs rm -v

RUN systemctl set-default multi-user.target

COPY setup /sbin/

STOPSIGNAL SIGRTMIN+3

# Using systemd as an entry point. It will launch mediaserver according to systemd scripts.
ENTRYPOINT ["/sbin/init", "--log-target=journal"]

CMD []

RUN mkdir /debs
COPY ${mediaserver_deb} /debs/

# Getting necessary packages for the server. There is some ancient tool that needs libssl1.0.0
RUN apt update -q && apt -y -qq install libssl1.0.0

# Installing the mediaserver
RUN apt install -y -qq --no-install-recommends /debs/${mediaserver_deb}
