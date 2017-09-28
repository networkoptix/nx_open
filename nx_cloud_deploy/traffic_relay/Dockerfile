FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:1.5

ADD stage/qt /opt/networkoptix/qt
ADD stage/traffic_relay /opt/networkoptix/traffic_relay

ADD etc/traffic_relay.conf /opt/networkoptix/traffic_relay/etc/traffic_relay.conf
ADD docker/entrypoint.sh /opt/networkoptix/traffic_relay/bin

EXPOSE 3349

ENV QT_PLUGIN_PATH /opt/networkoptix/qt/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/qt/lib:/opt/networkoptix/traffic_relay/lib

CMD ["/opt/networkoptix/traffic_relay/bin/entrypoint.sh"]

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
