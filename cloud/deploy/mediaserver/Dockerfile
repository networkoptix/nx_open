FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:3.0.2
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

COPY stage/mediaserver /opt/networkoptix/mediaserver

COPY docker/entrypoint.sh /opt/networkoptix/mediaserver/bin/
COPY docker/patch-cloud-host.sh docker/utils.sh /usr/local/bin/

ENV QT_PLUGIN_PATH=/opt/networkoptix/mediaserver/bin \
    LD_LIBRARY_PATH=/opt/networkoptix/mediaserver/lib

VOLUME ["/opt/networkoptix/mediaserver/etc", "/opt/networkoptix/mediaserver/var"]

WORKDIR "/opt/networkoptix/mediaserver"
ENTRYPOINT ["/usr/local/bin/dumb-init", "--", "/opt/networkoptix/mediaserver/bin/entrypoint.sh"]

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
