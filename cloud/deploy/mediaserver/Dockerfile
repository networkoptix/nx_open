FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

COPY stage/mediaserver /opt/networkoptix/mediaserver

COPY docker/entrypoint.sh /opt/networkoptix/mediaserver/bin

ENV QT_PLUGIN_PATH /opt/networkoptix/mediaserver/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/mediaserver/lib

VOLUME ["/opt/networkoptix/mediaserver/etc", "/opt/networkoptix/mediaserver/var"]

CMD ["/opt/networkoptix/mediaserver/bin/entrypoint.sh"]
