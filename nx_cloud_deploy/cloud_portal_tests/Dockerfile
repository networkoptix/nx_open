FROM debian:testing

RUN apt-get update && \
  apt-get install -y curl && \
  curl http://www.deb-multimedia.org/pool/main/d/deb-multimedia-keyring/deb-multimedia-keyring_2016.8.1_all.deb \
  -o /tmp/deb-multimedia-keyring_2015.6.1_all.deb && \
  dpkg -i /tmp/deb-multimedia-keyring_2015.6.1_all.deb && \
  rm /tmp/deb-multimedia-keyring_2015.6.1_all.deb && \
  echo "deb http://www.deb-multimedia.org stretch main non-free" >> /etc/apt/sources.list && \
  apt-get install -y \
    openjdk-8-jre \
    wget \
    xvfb \
    lsof \
    x11vnc \
    ratpoison \
    procps \
    net-tools \
    xterm \
    libgconf-2-4 \
    libexif12 \
    chromium \
    netcat-traditional \
    curl \
    ffmpeg \
    gnupg && \
  apt-get --reinstall install xfonts-base && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/*

# RUN wget -q -O - https://dl-ssl.google.com/linux/linux_signing_key.pub | apt-key add - 
# RUN sh -c 'echo "deb https://dl.google.com/linux/chrome/deb/ stable main" >> /etc/apt/sources.list.d/google.list'
# RUN apt-get update
# RUN apt-get install google-chrome-stable


RUN curl -sL https://deb.nodesource.com/setup_6.x | bash - && \
  apt-get update && \
  apt-get install -y nodejs && \
  rm -rf /var/lib/apt/lists/*

COPY stage /usr/src/app
WORKDIR /usr/src/app

RUN npm install
RUN npm install -g grunt
RUN node_modules/protractor/bin/webdriver-manager update

RUN useradd -m -s /bin/bash qa && chown -R qa /usr/src/app

COPY docker/vncpasswd /home/qa/.vnc/passwd
COPY docker/entrypoint.sh /entrypoint.sh
COPY docker/test-by-test.sh /test-by-test.sh
COPY docker/vncserver /usr/local/bin/

ENTRYPOINT ["/entrypoint.sh"]

ARG VERSION
ARG REVISION
ARG BUILD_DATE
ARG BUILD_HOST
ARG BUILD_USER
LABEL maintainer="Ivan Vigasin <ivigasin@gmail.com>" \
      version="$VERSION" \
      revision="$REVISION" \
      build-date="$BUILD_DATE" \
      build-host="$BUILD_HOST" \
      build-user="$BUILD_USER"
