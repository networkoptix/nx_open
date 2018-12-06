FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

ADD stage/qt /opt/networkoptix/qt
ADD stage/vms_gateway /opt/networkoptix/vms_gateway

ADD etc/vms_gateway.conf /opt/networkoptix/vms_gateway/etc/vms_gateway.conf
ADD docker/entrypoint.sh /opt/networkoptix/vms_gateway/bin

EXPOSE 3347

ENV QT_PLUGIN_PATH /opt/networkoptix/qt/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/qt/lib:/opt/networkoptix/vms_gateway/lib

CMD ["/opt/networkoptix/vms_gateway/bin/entrypoint.sh"]

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
