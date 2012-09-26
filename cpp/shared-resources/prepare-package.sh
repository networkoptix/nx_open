#!/bin/bash
export buildlib=${buildLib}
export QTDIR=`qmake -query QT_INSTALL_PREFIX`

if [[ $buildlib != 'staticlib' ]]; then

  mkdir -p ${libdir}/qtlibs/build/bin/debug
  mkdir -p ${libdir}/qtlibs/build/bin/release

  for i in ${qtlib1} ${qtlib2} ${qtlib3} ${qtlib4} ${qtlib5} ${qtlib6} ${qtlib7} ${qtlib8} ${qtlib9}
  do
    cp -P `find $QTDIR -iname 'libQt'"$i"'.so*'` ${libdir}/build/bin/debug
    cp -P `find $QTDIR -iname 'libQt'"$i"'.so*'` ${libdir}/build/bin/release  
  done

fi