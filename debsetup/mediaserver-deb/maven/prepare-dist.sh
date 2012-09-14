#!/bin/bash 

#unzip -a -o -u 'mediaserver*.zip'
#rm ./mediaserver*.zip
#unzip -a -o -u '*.zip' -x '**/include/**' '*.a'

#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done
#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done

sudo chown -R $USER:sudo ${project.build.directory}

chmod 755 ./init.d/*
chmod 755 ./debian/prerm 
chmod 755 ./debian/postinst
chmod 755 ./bin/mediaserver*
chmod 755 ./build-dist.sh
