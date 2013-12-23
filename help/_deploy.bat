@rem call mvn clean deploy -Dcustomization=nnodal
call mvn clean deploy -Dcustomization=digitalwatchdog
call mvn clean deploy 
call mvn clean deploy -Dchild.customization=relong -Dhelp.language=chinese