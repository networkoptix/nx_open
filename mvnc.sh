sudo rm -rf ./common/x86/build/debug
sudo rm -rf ./client/x86/build/debug
mvn compile --projects=common,client
