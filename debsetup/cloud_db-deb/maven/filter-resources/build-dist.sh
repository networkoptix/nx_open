#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}

VERSION=${release.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/cloud_db
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init

FINALNAME=${artifact.name.cdb}

STAGEBASE=deb
STAGE=$STAGEBASE/$FINALNAME
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET

SERVER_BIN_PATH=${libdir}/bin/${build.configuration}
SERVER_LIB_PATH=${libdir}/lib/${build.configuration}

# Prepare stage dir
rm -rf $STAGE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE

# Copy libraries
cp -P $SERVER_LIB_PATH/*.so* $LIBSTAGE
rm -f $LIBSTAGE/*.debug
#'libstdc++.so.6 is needed on some machines
cp -r /usr/lib/${arch.dir}/libstdc++.so.6* $LIBSTAGE

# Strip and remove rpath
if [ '${build.configuration}' == 'release' ]
then
  for f in `find $LIBSTAGE -type f`
  do
    strip $f
  done
fi

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
chmod -R 755 $BINSTAGE

install -m 755 $SERVER_BIN_PATH/cloud_db $BINSTAGE/cloud_db
install -m 644 init/networkoptix-cloud_db.conf $INITSTAGE/$COMPANY_NAME-cloud_db.conf
install -m 644 etc/cloud_db.conf $ETCSTAGE/cloud_db.conf

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
install -m 755 debian/prerm $STAGE/DEBIAN
install -m 755 debian/postinst $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b $FINALNAME)