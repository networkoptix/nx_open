# Get base image
FROM 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/base:latest

# Set the working directory
WORKDIR /

# Copy the current directory contents into the container
ADD . /

# Install some requirements for the container
RUN apt-get update && apt-get install -y \
    python python-pip curl unzip libgconf-2-4
RUN pip install --trusted-host pypi.python.org -r requirements.txt

RUN  apt-get update \
  && apt-get install -y wget \
  && rm -rf /var/lib/apt/lists/*

RUN  apt-get install -y libgconf-2-4
RUN  apt-get install -y libglib2.0-0

# Install chrome
RUN wget -q -O - https://dl-ssl.google.com/linux/linux_signing_key.pub | apt-key add -
RUN sh -c 'echo "deb [arch=amd64] http://dl.google.com/linux/chrome/deb/ stable main" >> /etc/apt/sources.list.d/google-chrome.list'
RUN apt-get -y update
RUN apt-get install -y google-chrome-stable

# Install chrome-driver
RUN apt-get install -yqq unzip
RUN wget chromedriver.storage.googleapis.com/2.36/chromedriver_linux64.zip
RUN unzip chromedriver_linux64.zip chromedriver
RUN mv chromedriver /usr/bin/chromedriver
RUN chown root:root /usr/bin/chromedriver
RUN chmod +x /usr/bin/chromedriver

# Make sure chrome is in the path and set path to the current directory too
RUN PATH=$PATH:/usr/lib/chromium-browser/
RUN PATH=$PATH:/


# Run all tests in English
CMD robot -v docker:true test-cases