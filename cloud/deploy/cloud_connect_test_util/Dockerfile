FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:3.0.2

COPY stage/cloud_connect_test_util/ /opt/networkoptix/cloud_connect_test_util/

COPY docker/nx_network.ini /tmp/nx_network.ini
COPY docker/patch-cloud-host.sh docker/utils.sh /usr/local/bin/
COPY docker/entrypoint.sh /opt/networkoptix/cloud_connect_test_util/bin/

ENV QT_PLUGIN_PATH=/opt/networkoptix/cloud_connect_test_util/bin \
    LD_LIBRARY_PATH=/opt/networkoptix/cloud_connect_test_util/lib

WORKDIR "/opt/networkoptix/cloud_connect_test_util"
ENTRYPOINT ["/usr/local/bin/dumb-init", "--", "/opt/networkoptix/cloud_connect_test_util/bin/entrypoint.sh"]

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
