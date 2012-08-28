#!/bin/bash
export buildlib=${buildLib}

if [[ $buildlib != 'staticlib' ]]; then

  mkdir -p ${libdir}/qtlibs/build/bin/debug
  mkdir -p ${libdir}/qtlibs/build/bin/release

  for i in ${qtlib1} ${qtlib2} ${qtlib3} ${qtlib4} ${qtlib5} ${qtlib6} ${qtlib7} ${qtlib8} ${qtlib9} 
  do
    cp -P `find ${qt.dir} -iname 'libQt'"$i"'.so*'` ${libdir}/build/bin/debug
    cp -P `find ${qt.dir} -iname 'libQt'"$i"'.so*'` ${libdir}/build/bin/release  
  done

#cd ${project.build.directory}/qtlibs

#zip -y -r ../qtlibs-${arch}.zip ./**

#cd ${project.build.directory}

  echo "export LD_LIBRARY_PATH=${libdir}/build/bin/${build.configuration}:/usr/lib" > ${libdir}/bin/${build.configuration}/env.sh
  mv ${libdir}/bin/${build.configuration}/${project.artifactId} ${libdir}/bin/${build.configuration}/${project.artifactId}-bin
fi