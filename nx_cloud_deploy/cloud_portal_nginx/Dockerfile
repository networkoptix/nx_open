FROM nginx:1.12.0-alpine
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

RUN apk add --no-cache apache2-utils

COPY dumb-init_1.2.0_amd64 /usr/local/bin/dumb-init
COPY nginx.conf.template /etc/nginx/conf.d/nginx.conf.template
COPY run_nginx.sh /run_nginx.sh

RUN chmod +x /usr/local/bin/dumb-init

ENV PORTAL_HOST=127.0.0.1

ENTRYPOINT ["/usr/local/bin/dumb-init", "--"]

STOPSIGNAL SIGKILL

CMD ["/run_nginx.sh"]
