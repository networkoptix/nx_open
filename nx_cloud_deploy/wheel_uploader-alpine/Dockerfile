FROM python:3.6-alpine3.7
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

RUN set -ex \
    && apk add --no-cache --virtual .build-deps \
            gcc \
            make \
            libc-dev \
            musl-dev \
            linux-headers \
            mariadb-dev \
            libffi-dev \
            yaml-dev \
            curl-dev \
    && apk add --no-cache nodejs-npm mariadb-client \
    && npm install -g yarn \
    && pip install -U pip \
    && pip install devpi-client>=2.3.0 wheel

WORKDIR /app
RUN mkdir wheelhouse

COPY entrypoint.sh /app/

CMD ["/app/entrypoint.sh"]
