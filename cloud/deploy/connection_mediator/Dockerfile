FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

ADD stage/qt /opt/networkoptix/qt
ADD stage/connection_mediator /opt/networkoptix/connection_mediator

ADD etc/connection_mediator.conf /opt/networkoptix/connection_mediator/etc/connection_mediator.conf
ADD docker/entrypoint.sh /opt/networkoptix/connection_mediator/bin

EXPOSE 3345

ENV QT_PLUGIN_PATH /opt/networkoptix/qt/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/qt/lib:/opt/networkoptix/connection_mediator/lib

CMD ["/opt/networkoptix/connection_mediator/bin/entrypoint.sh"]

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
