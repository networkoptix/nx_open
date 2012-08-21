sudo apt-get install -y openjdk-6-jre-headless protobuf-compiler build-essential unzip zip libqt4-dev libz-dev python-dev  libasound2 libxrender-dev libfreetype6-dev libfontconfig1-dev libxrandr-dev libxinerama-dev libxcursor-dev libopenal-dev

mkdir -p ~/environment
hg clone ssh://hg@noptix.enk.me/buildenv ~/environment
cd ~/environment
chmod 755 ./get_buildenv.sh
./get_buildenv.sh

echo 'export environment=~/environment' >> ~/.profile
echo 'export PATH=$environment/maven/bin:'$PATH >> ~/.profile
#echo 'export JAVA_HOME=/usr' >> ~/.profile

cd ~/environment
wget http://sourceforge.net/projects/cx-freeze/files/4.2.3/cx_Freeze-4.2.3.tar.gz/download?use_mirror=dfn
mv ./download\?use_mirror\=dfn cx_freeze.tar.gz
#tar xvf ./cx_freeze.tar.gz
#cd ./cx_Freeze*
#python setup.py build
#sudo python setup.py install
#rm -Rf ./cx_Freeze*

sudo cp ./appserver/patches/django_mgmt_init__.py /usr/local/lib/python2.7/dist-packages/django/core/management/__init__.py

cd ./appserver/

sudo python ./scripts/install_pip.py
sudo python ./scripts/install_others.py
