FROM alpine:3.5
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

RUN apk add --no-cache mysql-client

COPY tests /tests
WORKDIR /tests

CMD ["/tests/runtests.sh"]
