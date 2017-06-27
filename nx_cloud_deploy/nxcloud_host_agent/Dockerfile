FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/pybase:1.9

RUN mkdir -p /app
RUN virtualenv -p python3 /app/env

WORKDIR /app

COPY docker/pip.conf /root/.config/pip/pip.conf

COPY docker/docker /usr/local/bin
COPY docker/docker-machine /usr/local/bin
COPY docker/docker-compose /usr/local/bin
COPY docker/kubectl /usr/local/bin
COPY docker/aws_config /root/.aws/config
COPY docker/aws_credentials /root/.aws/credentials

ARG NXCLOUD_VERSION
ARG VERSION
ARG MODULES

RUN /app/env/bin/pip install --no-cache-dir nxcloud==${NXCLOUD_VERSION}

ENV PATH /app/env/bin:/usr/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV VERSION ${VERSION}
ENV MODULES "${MODULES}"

ENTRYPOINT ["/app/env/bin/nxcloud", "--mode=agent"]
