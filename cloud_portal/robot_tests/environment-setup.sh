
TESTENV=https://vm201.la.hdw.mx
SERVERVERSION=4.0.0.28057

robot -v ENV:$TESTENV special-cases/qa-user-creation
docker build -t mediaserver --build-arg mediaserver_deb=nxwitness-server-$SERVERVERSION*.deb .
for i in {1..7}
do
	docker-compose up
	robot -v sysname:system$i addsystem.robot
	docker-compose down
	rm -r video/
	rm -r data/

docker-compose up
robot -v sysname:"Auto Tests 2" addsystem.robot

docker-compose down
rm -r video/
rm -r data/