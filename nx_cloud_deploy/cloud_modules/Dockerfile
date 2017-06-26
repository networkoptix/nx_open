FROM alpine:3.5
MAINTAINER Ivan Vigasin <ivigasin@networkoptix.com>

COPY dumb-init_1.2.0_amd64 /usr/local/bin/dumb-init
COPY stage/cloud_modules /usr/local/bin/

RUN chmod +x /usr/local/bin/dumb-init

ENTRYPOINT ["/usr/local/bin/dumb-init", "--"]

CMD ["/usr/local/bin/cloud_modules"]
