
TESTENV=https://vm201.la.hdw.mx
SERVERVERSION=4.0.0.28541

#robot -v ENV:$TESTENV special-cases/qa-user-creation
docker build -t mediaserverma --build-arg mediaserver_deb=nxwitness-server-$SERVERVERSION*.deb .
a=0
while [ $a -lt 7 ]
do
    docker-compose up -d
    echo "running python"
    sleep  3
    python setupCloudSystem.py
    sleep  3
    docker-compose down
    rm -r video/
    rm -r data/
    a=`expr $a + 1`
done
#docker-compose up
#robot -v sysname:"Auto Tests 2" addsystem.robot

#docker-compose down
#rm -r video/
#rm -r data/
