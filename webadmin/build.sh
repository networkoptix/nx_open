#!/bin/bash

SOURCE_PATH=$1
# echo "clean sources directory affecting build process"
rm -R app
rm -R translation
rm -R translations

# echo "copy everything"
cp -Rf $SOURCE_PATH/webadmin/* .

sleep 20
# echo "install rvm if not installed"
rvm version || curl -sSL https://get.rvm.io | bash

#rvm install ruby-2.3.0
rvm use 2.3.0 --default

export PATH=$PATH:~/.rvm/bin:~/.rvm/rubies/ruby-2.3.0/bin:~/.rvm/gems/ruby-2.3.0/bin:$PWD/node_modules/.bin

echo "+++++++++++++++++++++++++ Running NPM... +++++++++++++++++++++++++"
npm install
if [ $? -gt 0 ]; then 
   echo "+++++++++++++++++++++++++ NPM errorlevel:" $? "+++++++++++++++++++++++++"
   exit $?
fi
~/.rvm/bin/rvm install 2.3.0
source ~/.rvm/scripts/rvm
rvm use 2.3.0 --default
gem install compass
#sleep 2
echo "+++++++++++++++++++++++++ Running BOWER... +++++++++++++++++++++++++"
bower install
if [ $? -gt 0 ]; then
   echo "+++++++++++++++++++++++++ BOWER errorlevel:" $? "+++++++++++++++++++++++++"
   exit $?
fi

# echo "copy bower components - it is a hack, we will fix it with new build process for webadmin"
cp -a bower_components app/bower_components/
#sleep 2
echo "+++++++++++++++++++++++++ Running GRUNT... +++++++++++++++++++++++++"
cp -f ./target/version_maven.txt ./static/


# echo "create version.txt for the build"
pushd $SOURCE_PATH
hg parent > version.txt
popd
mkdir static
mv $SOURCE_PATH/version.txt static/


# echo "run grunt build process"
grunt pub
if [ $? -gt 0 ]; then
   echo "+++++++++++++++++++++++++ GRUNT errorlevel:" $? "+++++++++++++++++++++++++"
   exit $?
fi
if [ ! -f ./external.dat ]; then rm ./target/rdep.sh && exit 1; fi
