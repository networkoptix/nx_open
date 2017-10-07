FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:1.5
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

COPY stage/mediaserver /opt/networkoptix/mediaserver

COPY docker/entrypoint.sh /opt/networkoptix/mediaserver/bin
COPY docker/etc /opt/networkoptix/mediaserver/etc/
COPY docker/var /opt/networkoptix/mediaserver/var/

ENV QT_PLUGIN_PATH /opt/networkoptix/mediaserver/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/mediaserver/lib

CMD ["/opt/networkoptix/mediaserver/bin/entrypoint.sh"]
