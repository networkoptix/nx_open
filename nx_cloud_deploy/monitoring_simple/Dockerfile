FROM python:3.5-alpine

ARG VERSION
ARG CACHE_DATE

RUN pip install --trusted-host la.hdw.mx --index http://la.hdw.mx:8006 --no-cache-dir monitoring_simple==${VERSION}

ENTRYPOINT [ "monitoring_simple" ]
