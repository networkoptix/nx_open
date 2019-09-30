
TESTENV=https://cloud-test.hdw.mx
SERVERVERSION=4.0.0.28541

#build the docker media server
docker build -t mediaserver --build-arg mediaserver_deb=nxwitness-server-$SERVERVERSION*.deb .
a=0
#create 7 eroneous offline systems for testing
while [ $a -lt 7 ]
do
    docker-compose up -d
    sleep  3
    python setupCloudSystem.py
    sleep  3
    docker-compose down
    sudo rm -rf video/
    sudo rm -rf data/
    echo $a
    a=`expr $a + 1`
done
#create the Auto Tests 2 system
docker-compose up -d
sleep  3
python setupCloudSystem.py "Auto Tests 2"
sleep  3
docker-compose down
sudo rm -r video/
sudo rm -r data/
