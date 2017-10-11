FROM python:3.6-alpine3.6

ARG VERSION
ARG CACHE_DATE

COPY stage/monitoring_simple-$VERSION-py3-none-any.whl /tmp

RUN pip install /tmp/monitoring_simple-$VERSION-py3-none-any.whl

ENTRYPOINT [ "monitoring_simple" ]
