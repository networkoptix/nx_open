# sudo apt-get install -y openjdk-6-jre-headless protobuf-compiler build-essential unzip zip libqt4-dev libz-dev python-dev libasound2

#mkdir -p ~/environment
#cd ~/environment

#hg clone ssh://hg@noptix.enk.me/buildenv ~/environment
#chmod 755 ~/environment/get_buildenv.sh
#~/environment/get_buildenv.sh

#mkdir maven
#cd ./maven
#wget --no-check-certificate https://boris:pizdohuj@noptix.enk.me/jenkins/maven.zip
#unzip ./maven.zip
#chmod 755 ~/environment/maven/bin/mvn

echo 'export environment=/drives/data2/QtSDK/projects/networkoptix/environment' >> ~/.profile
echo 'export PATH=/drives/data2/QtSDK/projects/networkoptix/environment/maven/bin:'$PATH >> ~/.profile
#echo 'export JAVA_HOME=/usr' >> ~/.profile

#cd ~/environment
#wget http://sourceforge.net/projects/cx-freeze/files/4.2.3/cx_Freeze-4.2.3.tar.gz/download?use_mirror=dfn
#mv ./download\?use_mirror\=dfn cx_freeze.tar.gz
#tar xvf ./cx_freeze.tar.gz
#cd ./cx_Freeze*
#python setup.py build
#sudo python setup.py install
#rm -Rf ./cx_Freeze*

sudo cp ./appserver/patches/django_mgmt_init__.py /usr/local/lib/python2.7/dist-packages/django/core/management/__init__.py

cd ./appserver/

sudo python ./scripts/install_pip.py
sudo python ./scripts/install_others.py
