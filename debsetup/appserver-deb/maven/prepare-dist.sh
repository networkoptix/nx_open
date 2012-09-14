#!/bin/bash 

#for f in ${project.build.directory}/*.zip 
#do 
#	unzip -a -o -u $f
#	rm $f
#done
#unzip -a -o -u '*.zip'

#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done
#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done

sudo chown -R $USER:sudo ${project.build.directory}

chmod 755 ./init.d/*
chmod 755 ./debian/prerm 
chmod 755 ./debian/postinst
chmod 755 ./bin/mediaproxy*
chmod 755 ./build-dist.sh
