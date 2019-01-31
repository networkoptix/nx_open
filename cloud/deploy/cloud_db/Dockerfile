FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

COPY stage/qt /opt/networkoptix/qt
COPY stage/cloud_db /opt/networkoptix/cloud_db

COPY etc/authorization_rules.xml /opt/networkoptix/cloud_db/etc/authorization_rules.xml
COPY etc/cloud_db.conf /opt/networkoptix/cloud_db/etc/cloud_db.conf
COPY etc/cloud_modules_template.xml /opt/networkoptix/cloud_db/etc/cloud_modules_template.xml
COPY etc/cloud_modules_new_template.xml /opt/networkoptix/cloud_db/etc/cloud_modules_new_template.xml
COPY docker/entrypoint.sh /opt/networkoptix/cloud_db/bin

EXPOSE 3346

ENV QT_PLUGIN_PATH /opt/networkoptix/qt/bin
ENV LD_LIBRARY_PATH /opt/networkoptix/qt/lib:/opt/networkoptix/cloud_db/lib

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
