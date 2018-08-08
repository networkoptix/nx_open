FROM golang:1.10.3-alpine3.7 AS builder

RUN apk --no-cache add git \
        && go get -u github.com/mcuadros/ofelia

# -------------------- 

FROM alpine:3.7

RUN apk --no-cache add tini

COPY --from=builder /go/bin/ofelia /usr/local/bin/
COPY entrypoint.sh /

ENTRYPOINT ["/sbin/tini", "--"]
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
