sudo apt-get install -y openjdk-6-jre-headless protobuf-compiler build-essential unzip libqt4-dev

mkdir -p ~/environment
cd ~/environment
wget --no-check-certificate https://boris:pizdohuj@noptix.enk.me/jenkins/maven.zip
mkdir maven
cd ./maven
unzip ./maven.zip
chmod 755 ~/environment/apache-maven-3.0.4/bin/mvn

echo 'export environment=~/environment' >> ~/.profile
echo 'export PATH=~/environment/apache-maven-3.0.4/bin/:'$PATH >> ~/.profile
echo 'export JAVA_HOME=/usr' >> ~/.profile
. ~/.profile

mkdir ~/projects

cd ~/projects/netoptix_vms/appserver/

sudo python ./scripts/install_pip.py
sudo python ./scripts/install_others.py

cd ~/projects/netoptix_vms