#unzip -a -o -u 'client*.zip'
#rm ./client*.zip
#unzip -a -o -u '*.zip' -x '**/include/**' '*.a'

#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done
#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done

mkdir -p ${project.build.directory}/usr/share/icons/hicolor
cp -Rf ${basedir}/../../cpp/shared-resources/icons/${custom.skin}/hicolor/* ${project.build.directory}/usr/share/icons/hicolor

sudo chown -R $USER:sudo ${project.build.directory}

chmod 775 ./bin/client*
chmod 775 ./build-dist.sh
