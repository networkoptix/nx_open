CUSTOMIZATIONS="airylife default digitalwatchdog ipera nnodal nutech pivothead relong win4net vista"

> urls.enk.txt
> urls.hdw.txt

for c in $CUSTOMIZATIONS
do
    mvn package -Dcustomization=$c -DbuildNumber=$1
    ./deploy.sh >> urls.enk.txt
    ./deploy.sh hdw >> urls.hdw.txt
done

