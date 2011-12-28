for i in common server appserver
do
  pushd $i
  python convert.py
  popd
done

# Workaround buggy qtservice
sed -i "" "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" server/build/Makefile.debug
sed -i "" "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" server/build/Makefile.debug
sed -i "" "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" server/build/Makefile.release
sed -i "" "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" server/build/Makefile.release

sed -i "" "s%^..\/build\/\(debug\/generated\/qtservice.moc\)%\1%" server/build/Makefile.debug
sed -i "" "s%^..\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" server/build/Makefile.debug
sed -i "" "s%^..\/build\/\(debug\/generated\/qtservice.moc\)%\1%" server/build/Makefile.release
sed -i "" "s%^..\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" server/build/Makefile.release

for i in common server 
do
  pushd $i/build
  make -f Makefile.debug -j9
  popd
done
