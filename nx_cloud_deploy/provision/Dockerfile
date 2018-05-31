FROM python:2.7-alpine3.7

RUN set -ex \
    apk update \
    && apk add --no-cache openssh-client ca-certificates \
    && apk add --no-cache --virtual .build-deps \
            gcc \
            make \
            libc-dev \
            musl-dev \
            linux-headers \
            libffi-dev \
            openssl-dev \
    && apk add libffi \
    && pip install cryptography ansible && \
        ansible-galaxy install dj-wasabi.telegraf rossmcdonald.influxdb ansiblebit.grafana \
    && runDeps="$( \
            scanelf --needed --nobanner --recursive /usr/local/lib/python2.7/site-packages \
                    | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
                    | sort -u \
                    | xargs -r apk info --installed \
                    | sort -u \
    )" \
    && echo "Run dependencies: $runDeps" \
    && apk add --virtual .python-rundeps $runDeps \
    && rm -rf /usr/local/share/.cache \
    && apk del .build-deps
    
COPY docker/merge_config.py /usr/local/bin/
COPY machine.py /machine.py
COPY playbooks /playbooks
COPY conf/ansible.cfg /etc/ansible/ansible.cfg
COPY docker/entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/playbooks/bootstrap.yml"]
