FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:3.0.3

COPY stage/storage_service /opt/networkoptix/storage_service
COPY etc/storage_service.conf /opt/networkoptix/etc/storage_service.conf
COPY docker/entrypoint.sh /opt/networkoptix/storage_service/bin

EXPOSE 3375 

ENV QT_PLUGIN_PATH=/opt/networkoptix/storage_service/bin \
    LD_LIBRARY_PATH=/opt/networkoptix/storage_service/lib

CMD ["/opt/networkoptix/storage_service/bin/entrypoint.sh"]

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
