FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

COPY docker/pip.conf /root/.config/pip/pip.conf
COPY docker/entrypoint.sh /app
COPY stage/cloud /app/app

ARG CACHE_DATE
COPY stage/cloud/requirements.txt /app/requirements.txt

RUN pip2 install -r /app/requirements.txt

ENV CLOUD_PORTAL_BASE_CONF_DIR /app/app/static
ENV C_FORCE_ROOT true

WORKDIR /app/app

RUN CUSTOMIZATION=default ./manage.py collectstatic --noinput

ENTRYPOINT ["/app/entrypoint.sh"]

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
