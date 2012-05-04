mkdir -p ${project.build.directory}/build/bin/debug
mkdir -p ${project.build.directory}/build/bin/release

for i in ${qtlib1} ${qtlib2} ${qtlib3} ${qtlib4} ${qtlib4} ${qtlib5} ${qtlib6} ${qtlib7} ${qtlib8} ${qtlib9}
do
  cp `find ${qt.dir} -iname 'libQt'"$i"'.so'` ${project.build.directory}/build/bin/debug 
  cp `find ${qt.dir} -iname 'libQt'"$i"'.so'` ${project.build.directory}/build/bin/release
done

for f in ${project.build.directory}/bin/release/${project.artifactId}*; do mv "$f" "$f-bin"; done

echo "export LD_LIBRARY_PATH=${project.build.directory}/build/bin/release:/usr/lib:${basedir}/../common/x64/bin/release" > ${project.build.directory}/bin/release/env.sh

chmod 775 ./init.d/*
chmod 775 ./debian/prerm 
chmod 775 ./debian/postinst
chmod 775 ./bin/mediaserver