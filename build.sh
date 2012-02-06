set -e

for i in common server client appserver
do
  pushd $i
  python convert.py
  popd
done

# Workaround buggy qtservice
if [ `uname -s` == 'Darwin' ]
then
    SED_ARGS='-i .bak'
else
    # Assume GNU sed
    SED_ARGS='-i.bak'
fi

sed $SED_ARGS "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" server/build/Makefile.debug
sed $SED_ARGS "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" server/build/Makefile.debug
sed $SED_ARGS "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" server/build/Makefile.release
sed $SED_ARGS "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" server/build/Makefile.release

sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice.moc\)%\1%" server/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" server/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice.moc\)%\1%" server/build/Makefile.release
sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" server/build/Makefile.release

sed $SED_ARGS "s%\.\.\/build\/debug%debug%g" client/build/Makefile.debug
sed $SED_ARGS "s%\.\.\/build\/release%release%g" client/build/Makefile.release

rm server/build/Makefile.debug.bak server/build/Makefile.release.bak client/build/Makefile.debug.bak client/build/Makefile.release.bak

for i in common server client
do
  pushd $i/build
  make -f Makefile.debug -j9
  make -f Makefile.debug -j9
  popd
done
