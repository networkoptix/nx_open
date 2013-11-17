#!/bin/bash -x

if [ $(id -u) -eq 0 ]
then
    SSH_FIX_FILE="/etc/sudoers.d/root_ssh_agent"
    if [ ! -f  $SSH_FIX_FILE ]
    then
        echo "Defaults env_keep += \"SSH_AUTH_SOCK\"" > $SSH_FIX_FILE
        chmod 0440 $SSH_FIX_FILE
    fi

    apt-get update
    apt-get install -y python-software-properties mercurial vim curl openjdk-7-jre-headless protobuf-compiler build-essential unzip zip libz-dev python-dev libasound2 libxrender-dev libfreetype6-dev libfontconfig1-dev libxrandr-dev libxinerama-dev libxcursor-dev libopenal-dev mesa-common-dev freeglut3 freeglut3-dev libglu1-mesa-dev chrpath python-virtualenv screen libogg-dev libaudio2 libxi6 libxslt1.1

    add-apt-repository ppa:pi-rho/dev
    add-apt-repository http://enk.me/dw

    apt-get update
    apt-get install -y tmux

    su - vagrant $0
    exit 0
fi

# This part should be run as vagrant user
WSDIR=$HOME/workspace

cp /vagrant/.vagrant/.hgrc ~
cp /vagrant/.vagrant/.vimrc ~

grep environment ~/.profile > /dev/null 2>&1 || {
echo "" >> ~/.profile
cat >> ~/.profile << EOF

# Network optix environment
export environment=$WSDIR/buildenv
export PATH=\$environment/maven/bin:\$environment/python/bin:\$environment/qt/bin:\$PATH
export QTDIR=\$environment/qt
export JAVA_HOME=/usr
# End of network optix environment
EOF
}

mkdir $WSDIR

pushd $WSDIR

hg clone ssh://hg@noptix.enk.me/hg/buildenv
pushd buildenv
# ./get_buildenv.sh
popd

hg clone /vagrant netoptix_vms
cp /vagrant/.hg/hgrc netoptix_vms/.hg

popd

