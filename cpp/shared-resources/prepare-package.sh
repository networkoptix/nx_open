mkdir -p ${project.build.directory}/qtlibs/build/bin/debug
mkdir -p ${project.build.directory}/qtlibs/build/bin/release

for i in ${qtlib1} ${qtlib2} ${qtlib3} ${qtlib4} ${qtlib5} ${qtlib6} ${qtlib7} ${qtlib8} ${qtlib9} 
do
  cp -P `find ${qt.dir} -iname 'libQt'"$i"'.so*'` ${project.build.directory}/qtlibs/build/bin/debug
  cp -P `find ${qt.dir} -iname 'libQt'"$i"'.so*'` ${project.build.directory}/qtlibs/build/bin/release  
done

cd ${project.build.directory}/qtlibs

zip -y -r ../qtlibs-x86.zip ./**

cd ${project.build.directory}

mv ${project.build.directory}/bin/${build.configuration}/${project.artifactId} ${project.build.directory}/bin/${build.configuration}/${project.artifactId}-bin

echo "export LD_LIBRARY_PATH=${project.build.directory}/build/bin/${build.configuration}:/usr/lib:${basedir}/../common/${arch}/bin/${build.configuration}" > ${project.build.directory}/bin/${build.configuration}/env.sh