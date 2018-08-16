FROM nginx:1.14.0-alpine

RUN apk add --no-cache apache2-utils tini

COPY nginx.conf.template /etc/nginx/conf.d/nginx.conf.template
COPY run_nginx.sh /run_nginx.sh

ENV PORTAL_HOST=127.0.0.1

ENTRYPOINT ["/sbin/tini", "--"]

STOPSIGNAL SIGKILL

CMD ["/run_nginx.sh"]

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
