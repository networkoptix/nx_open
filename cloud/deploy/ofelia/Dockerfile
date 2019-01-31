FROM golang:1.10.3-alpine3.7 AS builder

ARG VERSION

RUN apk --no-cache add git \
        && go get -u github.com/vigasin/ofelia github.com/awslabs/amazon-ecr-credential-helper/ecr-login/cli/docker-credential-ecr-login

# -------------------- 

FROM alpine:3.7

RUN apk --no-cache add tini jq ca-certificates

COPY --from=builder /go/bin/ofelia /go/bin/docker-credential-ecr-login /usr/local/bin/
COPY docker_config.json /root/.docker/config.json
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
