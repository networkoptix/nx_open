sudo apt-get install -y openjdk-6-jre-headless protobuf-compiler build-essential unzip zip libqt4-dev libz-dev python-dev

mkdir -p ~/environment
cd ~/environment

mkdir maven
cd ./maven
wget --no-check-certificate https://boris:pizdohuj@noptix.enk.me/jenkins/maven.zip
unzip ./maven.zip
chmod 755 ~/environment/maven/bin/mvn

echo 'export environment=~/environment' >> ~/.profile
echo 'export PATH=~/environment/maven/bin:'$PATH >> ~/.profile
echo 'export JAVA_HOME=/usr' >> ~/.profile

cd ~/environment
wget http://sourceforge.net/projects/cx-freeze/files/4.2.3/cx_Freeze-4.2.3.tar.gz/download?use_mirror=dfn
mv ./download\?use_mirror\=dfn cx_freeze.tar.gz
tar xvf ./cx_freeze.tar.gz
cd ./cx_Freeze*
python setup.py build
sudo python setup.py install
rm -Rf ./cx_Freeze*

sudo cp ~/projects/netoptix_vms/appserver/patches/django_mgmt_init__.py /usr/local/lib/python2.7/dist-packages/django/core/management/__init__.py

cd ~/projects/netoptix_vms/appserver/

sudo python ./scripts/install_pip.py
sudo python ./scripts/install_others.py
