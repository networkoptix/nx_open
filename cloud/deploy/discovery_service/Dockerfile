FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:3.0.3

COPY stage/discovery_service /opt/networkoptix/discovery_service

COPY docker/entrypoint.sh /opt/networkoptix/discovery_service/bin

EXPOSE 3367

CMD ["/opt/networkoptix/discovery_service/bin/entrypoint.sh"]

ARG VERSION
ARG REVISION
ARG BUILD_DATE
ARG BUILD_HOST
ARG BUILD_USER
LABEL maintainer="Andrey Kolesnikov <akolesnikov@networkoptix.com>" \
      version="$VERSION" \
      revision="$REVISION" \
      build-date="$BUILD_DATE" \
      build-host="$BUILD_HOST" \
      build-user="$BUILD_USER"
