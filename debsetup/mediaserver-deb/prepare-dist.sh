unzip -a -o -u 'mediaserver*.zip'
rm ./mediaserver*.zip
unzip -a -o -u '*.zip'

#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done
#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done

sudo chown -R `whoami`:sudo ${project.build.directory}

chmod 775 ./init.d/*
chmod 775 ./debian/prerm 
chmod 775 ./debian/postinst
chmod 775 ./bin/mediaserver*
chmod 775 ./build-dist.sh