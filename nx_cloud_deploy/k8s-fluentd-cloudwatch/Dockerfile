FROM fluent/fluentd:v0.12.36
MAINTAINER Eljas Alakulppi <eljas@callstats.io>

USER root
RUN apk --no-cache --update add sudo build-base ruby-dev && \
    gem install --no-rdoc --no-ri aws-sdk-core -v "=2.9.32" && \
    gem install --no-rdoc --no-ri fluent-plugin-cloudwatch-logs && \
    gem install --no-rdoc --no-ri fluent-plugin-kubernetes_metadata_filter && \
    rm -rf /usr/lib/ruby/gems/2.3.0/cache/*.gem  && gem sources -c && \
    apk del sudo build-base ruby-dev && rm -rf /var/cache/apk/*

ADD fluent.conf /fluentd/etc/
ADD entrypoint.sh /bin/

ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ["sh", "-c", "fluentd -c /fluentd/etc/$FLUENTD_CONF -p /fluentd/plugins $FLUENTD_OPT"]
