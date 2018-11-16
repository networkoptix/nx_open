FROM alpine:3.5

COPY dumb-init_1.2.0_amd64 /usr/local/bin/dumb-init
COPY stage/cloud_modules /usr/local/bin/

RUN chmod +x /usr/local/bin/dumb-init

ENTRYPOINT ["/usr/local/bin/dumb-init", "--"]

CMD ["/usr/local/bin/cloud_modules"]

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
