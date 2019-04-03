
TESTENV=https://cloud-test.hdw.mx
SERVERVERSION=4.0.0.28541

#robot -v ENV:$TESTENV special-cases/qa-user-creation
docker build -t mediaserverma --build-arg mediaserver_deb=nxwitness-server-$SERVERVERSION*.deb .
a=0
while [ $a -lt 7 ]
do
    docker-compose up -d
    sleep  3
    python setupCloudSystem.py
    sleep  90
    docker-compose down
    sudo rm -rf video/
    sudo rm -rf data/
    echo $a
    a=`expr $a + 1`

done
docker-compose up
sleep  3
python setupCloudSystem.py "Auto Tests 2"
sleep  90
docker-compose down
rm -r video/
rm -r data/
