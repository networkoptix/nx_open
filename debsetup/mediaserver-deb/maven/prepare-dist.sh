#!/bin/bash 

#unzip -a -o -u 'mediaserver*.zip'
#rm ./mediaserver*.zip
#unzip -a -o -u '*.zip' -x '**/include/**' '*.a'

#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done
#for f in ${project.build.directory}/bin/${build.configuration}/${project.artifactId}*; do mv "$f" "$f-bin"; done

#cp -P `find ${qt.dir} -iname 'libQtDBus.so*'` ${libdir}/build/bin/debug
#cp -P `find ${qt.dir} -iname 'libQtDBus.so*'` ${libdir}/build/bin/release
#cp -P `find ${qt.dir} -iname 'libqgenericbearer.so*'` ${libdir}/build/bin/debug
#cp -P `find ${qt.dir} -iname 'libqgenericbearer.so*'` ${libdir}/build/bin/release

sudo chown -R $USER:sudo ${project.build.directory}

chmod 755 ./init.d/*
chmod 755 ./debian/prerm 
chmod 755 ./debian/postinst
chmod 755 ./bin/mediaserver*
chmod 755 ./build-dist.sh
