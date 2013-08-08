#!/bin/bash -x

if [ $(id -u) -eq 0 ]
then
    apt-get update
    apt-get install -y mercurial vim curl openjdk-7-jre-headless protobuf-compiler build-essential unzip zip libz-dev python-dev libasound2 libxrender-dev libfreetype6-dev libfontconfig1-dev libxrandr-dev libxinerama-dev libxcursor-dev libopenal-dev mesa-common-dev freeglut3 freeglut3-dev libglu1-mesa-dev chrpath python-virtualenv screen libogg-dev libaudio2 libxi6 libxslt1

    su - vagrant $0
    exit 0
fi

# This part should be run as vagrant user
WSDIR=$HOME/workspace

[ -f ~/.hgrc ] || cat > ~/.hgrc << EOF
[ui]
username = My Name <my@email.com>
verbose = True

[extensions]
eol =
extdiff = 
purge = 
mq =
fetch = 
EOF

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

cat >> ~/.vimrc << EOF
syntax on
set expandtab
set tabstop=4
set shiftwidth=4
set ai
set ruler
set hlsearch
set nobomb 
EOF

mkdir $WSDIR

pushd $WSDIR

hg clone http://noptix.enk.me/hg/buildenv
pushd buildenv
./get_buildenv.sh
popd

ln -s /vagrant netoptix_vms
popd

