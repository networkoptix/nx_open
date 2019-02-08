FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest
MAINTAINER Ivan Vigasin <ivigasin@gmail.com>

COPY docker/01proxy /etc/apt/apt.conf.d/01proxy
RUN add-apt-repository -y ppa:nginx/stable && \
    add-apt-repository ppa:jonathonf/python-3.6 && \
        apt-get update && apt-get install -y --force-yes --no-install-recommends \
            build-essential \
            git \
            python \
            python-dev \
            python3.6 \
            python3.6-dev \
            nginx-extras \
            libncurses5 \
            libmysqlclient-dev \
            libcurl3 \
            libcurl4-gnutls-dev && \
            rm -rf /var/lib/apt/lists/*

COPY docker/get-pip.py /
RUN python2 get-pip.py && \
    python3 get-pip.py && \
    pip2 install supervisor==3.3.1 supervisor-stdout==0.1.1 virtualenv && \
    rm /get-pip.py

# restart nginx to load the config
RUN service nginx stop

# expose port(s)
EXPOSE 80
