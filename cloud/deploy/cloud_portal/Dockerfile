FROM python:3.7-alpine3.7

COPY docker/merge_config.py /usr/local/bin/
COPY docker/pip.conf /root/.config/pip/pip.conf
COPY docker/entrypoint.sh /app/
COPY stage/cloud /app/app

ARG CACHE_DATE
COPY stage/cloud/requirements.txt /app/requirements.txt

RUN set -ex \
    apk update \
    && apk add --no-cache bash tini gettext wget ca-certificates mariadb-client   \
        libcurl libjpeg-turbo libressl2.6-libcrypto libwebp libxml2 libxslt mariadb-client-libs musl openjpeg tiff yaml zlib \
    && pip install -r /app/requirements.txt

ENV CLOUD_PORTAL_BASE_CONF_DIR /app/app/static
ENV C_FORCE_ROOT true

WORKDIR /app/app

RUN CUSTOMIZATION=default ./manage.py collectstatic --noinput

ENTRYPOINT ["/sbin/tini", "--", "/app/entrypoint.sh"]

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
