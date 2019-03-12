ARG VERSION
FROM python:${VERSION}
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

RUN set -ex \
    && apk add --no-cache --virtual .build-deps \
            gcc \
            g++ \
            make \
            libc-dev \
            musl-dev \
            linux-headers \
            mariadb-dev \
            postgresql-dev \
            libffi-dev \
            yaml-dev \
            curl-dev \
            libxml2-dev \
            libxslt-dev \
            zlib-dev \
            tiff-dev \
            freetype \
            libwebp-dev \
            openjpeg-dev \
    && apk add --no-cache nodejs-npm mariadb-client \
    && npm install -g yarn \
    && pip install -U pip \
    && pip install devpi-client>=2.3.0 wheel \
    && ln -sf /usr/include/libxml2/libxml /usr/include/libxml

WORKDIR /app
RUN mkdir wheelhouse

COPY entrypoint.sh /app/

CMD ["/app/entrypoint.sh"]
