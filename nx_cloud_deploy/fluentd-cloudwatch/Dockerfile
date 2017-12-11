FROM fluent/fluentd:v0.12.41
MAINTAINER Eljas Alakulppi <eljas@callstats.io>

USER root
RUN apk --no-cache --update add git sudo build-base ruby-dev && \
    gem install --no-rdoc --no-ri specific_install && \
    gem install --no-rdoc --no-ri aws-sdk-cloudwatchlogs -v "=1.2.0" && \
    gem specific_install -l https://github.com/vigasin/fluent-plugin-cloudwatch-logs -r b67d89e3243c25ea52e9b83e2fd3399a3a93c042 && \
    gem install --no-rdoc --no-ri fluent-plugin-kubernetes_metadata_filter && \
    rm -rf /usr/lib/ruby/gems/2.3.0/cache/*.gem  && gem sources -c && \
    apk del sudo build-base ruby-dev git && rm -rf /var/cache/apk/*

ADD fluent.conf /fluentd/etc/
ADD entrypoint.sh /bin/

ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ["sh", "-c", "fluentd -c /fluentd/etc/$FLUENTD_CONF -p /fluentd/plugins $FLUENTD_OPT"]
