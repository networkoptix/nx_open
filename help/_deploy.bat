@rem call mvn clean deploy -Dcustomization=nnodal
call mvn clean package -Dcustomization=default
copy target\*.gz ..\
call mvn clean package -Dcustomization=digitalwatchdog
copy target\*.gz ..\
call mvn clean package -Dcustomization=ipera
copy target\*.gz ..\
call mvn clean package -Dcustomization=nnodal
copy target\*.gz ..\
call mvn clean package -Dcustomization=nutech
copy target\*.gz ..\
call mvn clean package -Dcustomization=pcms
copy target\*.gz ..\
call mvn clean package -Dcustomization=vista
copy target\*.gz ..\
call mvn clean package -Dcustomization=vmsdemoblue
copy target\*.gz ..\
call mvn clean package -Dcustomization=vmsdemoorange
copy target\*.gz ..\