FROM golang:1.10.3-alpine3.7 as builder
RUN apk add --no-cache git \
        && go get -u github.com/awslabs/amazon-ecr-credential-helper/ecr-login/cli/docker-credential-ecr-login

FROM python:3.7-alpine3.7

COPY --from=builder /go/bin/docker-credential-ecr-login /usr/local/bin/
COPY docker/pip.conf /root/.config/pip/pip.conf

COPY docker/config.json /root/.docker/config.json
COPY docker/aws_config /root/.aws/config
COPY docker/aws_credentials /root/.aws/credentials
COPY nxcloud_run_config.yaml /root/nxcloud_run_config.yaml

ARG NXCLOUD_VERSION
ARG VERSION
ARG MODULES

RUN set -ex \
    && pip install --no-cache-dir --force-reinstall pip==9.0.1 \
    && pip install --no-cache-dir docker-compose==1.21.0 nxcloud==${NXCLOUD_VERSION} \
    && rm -rf /usr/local/share/.cache \
    && apk add --no-cache tini wget curl ca-certificates docker \
        libffi \
        libressl2.6-libcrypto \
        libressl2.6-libssl \
        musl \
        yaml \
    && for f in kubectl docker-machine; do curl -L -o /usr/local/bin/$f http://la.hdw.mx/devtools/$f/latest/$f; chmod 755 /usr/local/bin/$f; done

ENV NXCLOUD_RUN_CONFIG /root/nxcloud_run_config.yaml
ENV PATH /usr/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV VERSION ${VERSION}
ENV MODULES "${MODULES}"

ENTRYPOINT ["/sbin/tini", "--", "nxcloud", "--mode=agent"]

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
