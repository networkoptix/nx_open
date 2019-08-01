Jetson OpenALPR DeepStream Plugin
===================================

To install OpenALPR SDK:
-------------------------

    sudo su
    curl -L https://deb.openalpr.com/openalpr.gpg.key | apt-key add -
    echo "deb https://deb.openalpr.com/xenial-commercial-beta/ xenial main" > /etc/apt/sources.list.d/openalpr.list
    apt-get update && apt-get install -y openalpr libopenalpr-dev libalprstream-dev libvehicleclassifier-dev

Request an evaluation license key from https://license.openalpr.com
Paste the key in /etc/openalpr/license.conf


To compile this plug-in:
-------------------------

    git clone git@github.com:openalpr/deepstream_jetson.git
    cd deepstream_jetson
    make && sudo make install


To run this plug-in within Nvidia DeepStream, please refer to Nvidia DeepStream documentation, specifically running the nvgstiva-app.

Licensing
----------

The plug-in code is provided under the terms of the Nvidia Software License Agreement included with the DeepStream SDK.

