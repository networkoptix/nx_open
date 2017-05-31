FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/pybase:1.9
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

RUN virtualenv /app/env

COPY docker/pip.conf /root/.config/pip/pip.conf
COPY docker/entrypoint.sh /app
COPY stage/cloud /app/app

ARG CACHE_DATE
COPY stage/cloud/requirements.txt /app/requirements.txt

ENV PATH /app/env/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
RUN pip install -r /app/requirements.txt

ENV CLOUD_PORTAL_BASE_CONF_DIR /app/app/static
ENV C_FORCE_ROOT true

WORKDIR /app/app

RUN CUSTOMIZATION=default ./manage.py collectstatic --noinput

ENTRYPOINT ["/app/entrypoint.sh"]
