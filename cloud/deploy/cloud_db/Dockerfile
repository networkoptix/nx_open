FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

COPY stage/cloud_db /opt/networkoptix/cloud_db

COPY etc/authorization_rules.xml \
     etc/cloud_db.conf \
     etc/cloud_modules_template.xml \
     etc/cloud_modules_new_template.xml \
     /opt/networkoptix/cloud_db/etc/

COPY docker/entrypoint.sh /opt/networkoptix/cloud_db/bin

EXPOSE 3346

ENV QT_PLUGIN_PATH /opt/networkoptix/cloud_db/bin \
    LD_LIBRARY_PATH /opt/networkoptix/cloud_db/lib

CMD ["/opt/networkoptix/cloud_db/bin/entrypoint.sh"]

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
