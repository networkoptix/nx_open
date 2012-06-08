# Workaround buggy qtservice
if [ `uname -s` == 'Dawrin' ]
then
    SED_ARGS='-i .bak'
else
    # Assume GNU sed
    SED_ARGS='-i.bak'
fi

sed $SED_ARGS "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" mediaserver/build/Makefile.debug
sed $SED_ARGS "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" mediaserver/build/Makefile.debug
sed $SED_ARGS "1,/release\/generated\/qtservice.moc\ \\\\/{/release\/generated\/qtservice.moc\\ \\\\/d;}" mediaserver/build/Makefile.release
sed $SED_ARGS "1,/release\/generated\/qtservice_unix.moc\ \\\\/{/release\/generated\/qtservice_unix.moc\\ \\\\/d;}" mediaserver/build/Makefile.release

sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice.moc\)%\1%" mediaserver/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" mediaserver/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(release\/generated\/qtservice.moc\)%\1%" mediaserver/build/Makefile.release
sed $SED_ARGS "s%^\.\.\/build\/\(release\/generated\/qtservice_unix.moc\)%\1%" mediaserver/build/Makefile.release


sed $SED_ARGS "1,/debug\/generated\/qtservice.moc\ \\\\/{/debug\/generated\/qtservice.moc\\ \\\\/d;}" mediaproxy/build/Makefile.debug
sed $SED_ARGS "1,/debug\/generated\/qtservice_unix.moc\ \\\\/{/debug\/generated\/qtservice_unix.moc\\ \\\\/d;}" mediaproxy/build/Makefile.debug
sed $SED_ARGS "1,/release\/generated\/qtservice.moc\ \\\\/{/release\/generated\/qtservice.moc\\ \\\\/d;}" mediaproxy/build/Makefile.release
sed $SED_ARGS "1,/release\/generated\/qtservice_unix.moc\ \\\\/{/release\/generated\/qtservice_unix.moc\\ \\\\/d;}" mediaproxy/build/Makefile.release

sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice.moc\)%\1%" mediaproxy/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(debug\/generated\/qtservice_unix.moc\)%\1%" mediaproxy/build/Makefile.debug
sed $SED_ARGS "s%^\.\.\/build\/\(release\/generated\/qtservice.moc\)%\1%" mediaproxy/build/Makefile.release
sed $SED_ARGS "s%^\.\.\/build\/\(release\/generated\/qtservice_unix.moc\)%\1%" mediaproxy/build/Makefile.release

sed $SED_ARGS "s%\.\.\/build\/debug%debug%g" client/build/Makefile.debug
sed $SED_ARGS "s%\.\.\/build\/release%release%g" client/build/Makefile.release

