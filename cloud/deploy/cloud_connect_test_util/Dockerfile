FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:3.0.2

COPY stage/ /opt/networkoptix/cloud_connect_test_util/

COPY docker/nx_network.ini /tmp/nx_network.ini

ENV QT_PLUGIN_PATH=/opt/networkoptix/cloud_connect_test_util/bin \
    LD_LIBRARY_PATH=/opt/networkoptix/cloud_connect_test_util/lib

ENTRYPOINT ["/usr/local/bin/dumb-init", "--", "/opt/networkoptix/cloud_connect_test_util/bin/cloud_connect_test_util"]

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
