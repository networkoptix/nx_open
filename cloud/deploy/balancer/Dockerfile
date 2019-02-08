FROM nginx:1.12.2
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

COPY docker/nginx.conf /etc/nginx/nginx.conf
COPY docker/nginx.tmpl /etc/docker-gen/templates/nginx.tmpl
COPY docker/vhost.d /etc/nginx/vhost.d/
COPY docker/entrypoint.sh /
COPY docker/dumb-init_1.2.0_amd64 /usr/local/bin/dumb-init

RUN chmod +x /usr/local/bin/dumb-init

ENTRYPOINT ["/usr/local/bin/dumb-init", "--"]
CMD ["/entrypoint.sh"]

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
