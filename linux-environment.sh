sudo apt-get install -y openjdk-6-jre-headless protobuf-compiler build-essential unzip libqt4-dev libz-dev

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


mkdir ~/projects

cd ~/projects/netoptix_vms/appserver/

sudo python ./scripts/install_pip.py
sudo python ./scripts/install_others.py

cd ~/projects/netoptix_vms

. ~/.profile

sudo chown -R 