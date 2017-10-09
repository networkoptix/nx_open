FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

RUN mkdir -p /app

WORKDIR /app

COPY docker/pip.conf /root/.config/pip/pip.conf

COPY docker/docker /usr/local/bin
COPY docker/docker-machine /usr/local/bin
COPY docker/docker-compose /usr/local/bin
COPY docker/kubectl /usr/local/bin
COPY docker/aws_config /root/.aws/config
COPY docker/aws_credentials /root/.aws/credentials
COPY nxcloud_run_config.yaml /root/nxcloud_run_config.yaml

ARG NXCLOUD_VERSION
ARG VERSION
ARG MODULES

RUN pip3 install --no-cache-dir nxcloud==${NXCLOUD_VERSION}

ENV NXCLOUD_RUN_CONFIG /root/nxcloud_run_config.yaml
ENV PATH /usr/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV VERSION ${VERSION}
ENV MODULES "${MODULES}"

ENTRYPOINT ["nxcloud", "--mode=agent"]

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
