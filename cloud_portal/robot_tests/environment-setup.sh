
TESTENV=https://vm201.la.hdw.mx
SERVERVERSION=4.0.0.28057

robot -v ENV:$TESTENV special-cases/qa-user-creation
docker build -t mediaserver --build-arg mediaserver_deb=nxwitness-server-$SERVERVERSION*.deb .
for i in {1..8}
do
	docker-compose up
	robot addsystem.robot
	docker-compose down
	sudo rm -r video/
	sudo rm -r data/
