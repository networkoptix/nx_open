#!/bin/bash -x

if [ $(id -u) -eq 0 ]
then
    apt-get update
    apt-get install -y mercurial vim curl openjdk-7-jre-headless protobuf-compiler build-essential unzip zip libz-dev python-dev libasound2 libxrender-dev libfreetype6-dev libfontconfig1-dev libxrandr-dev libxinerama-dev libxcursor-dev libopenal-dev mesa-common-dev freeglut3 freeglut3-dev libglu1-mesa-dev chrpath python-virtualenv screen libogg-dev

    su - vagrant $0
    exit 0
fi

# This part should be run as vagrant user
WSDIR=$HOME/workspace

[ -f ~/.hgrc ] || cat > ~/.hgrc << EOF
username=<Your Name> <yourmail@host.com>
[extensions]
eol =
hgext.purge=
EOF

grep environment ~/.profile > /dev/null 2>&1 || {
echo "" >> ~/.profile
cat >> ~/.profile << EOF

# Network optix environment
export environment=$WSDIR/buildenv
export PATH=\$environment/maven/bin:\$environment/qt/bin:\$PATH
export QTDIR=\$environment/qt
export JAVA_HOME=/usr
# End of network optix environment
EOF
}

mkdir $WSDIR

pushd $WSDIR

hg clone http://noptix.enk.me/hg/buildenv
pushd buildenv
./get_buildenv.sh
popd

ln -s /vagrant netoptix_vms
popd

