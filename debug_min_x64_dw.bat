mvn compile --projects build_environment -Dbuild.configuration=debug -Darch=x64 -Dcustomization=digitalwatchdog 
mvn compile --projects common,client -Dbuild.configuration=debug -Darch=x64 -Dcustomization=digitalwatchdog 
