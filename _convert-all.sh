mvn compile --projects=build_environment
mvn compile -T 4 --projects=appserver,common,applauncher,client,mediaserver,mediaproxy

