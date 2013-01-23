mvn compile --projects=build-environment
mvn compile -T 4 --projects=appserver,common,client,mediaserver
