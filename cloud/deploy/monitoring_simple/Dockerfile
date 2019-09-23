FROM python:3.7-alpine3.10

ARG VERSION
ARG CACHE_DATE

RUN set -ex \
       apk update \
	&& apk add --no-cache ca-certificates libcrypto1.1 libffi libssl1.1 \
    && apk add --no-cache --virtual build-dependencies gcc g++ musl-dev go git \
    && export GOPATH=/go \
    && export PATH=$GOPATH/bin:$PATH \
    && mkdir $GOPATH \
    && chmod -R 777 $GOPATH \
    && APP_REPO=github.com/awslabs/amazon-ecr-credential-helper \
    && APP_REV=0e5718c95e53137f56b449798d259bc457bb4aa6 \
    && git clone https://$APP_REPO $GOPATH/src/$APP_REPO \
    && cd $GOPATH/src/$APP_REPO \
    && git checkout $APP_REV \
    && GOOS=linux CGO_ENABLED=0 go build -installsuffix cgo \
       -a -ldflags '-s -w' -o /usr/bin/docker-credential-ecr-login \
       ./ecr-login/cli/docker-credential-ecr-login \
    && cd / \
    && apk del --purge -r build-dependencies \
    && rm -rf /go
    
COPY pip.conf /root/.config/pip/pip.conf
COPY docker_config.json /root/.docker/config.json
COPY stage/monitoring_simple-$VERSION-py3-none-any.whl /tmp

RUN sed -i'' "s/\$VERSION/3.7.3-alpine3.10/" /root/.config/pip/pip.conf \
    && pip install /tmp/monitoring_simple-$VERSION-py3-none-any.whl

ENTRYPOINT [ "monitoring_simple" ]

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
