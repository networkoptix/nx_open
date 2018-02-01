#!/bin/bash

SOURCE_PATH=$1
# echo "clean sources directory affecting build process"
rm -R app
rm -R translation
rm -R translations

# echo "copy everything"
cp -Rf $SOURCE_PATH/webadmin/* .

# echo "clean build directory - moved from Gruntfile.js"
rm -R static
mkdir static # Make sure directory exists


rvm version || ( echo "Error: rvm not installed" && exit $? )

#rvm install ruby-2.3.0
rvm use 2.3.0 --default

# export PATH=$PATH:~/.rvm/bin:~/.rvm/rubies/ruby-2.3.0/bin:~/.rvm/gems/ruby-2.3.0/bin:$PWD/node_modules/.bin
# ~/.rvm/bin/rvm install 2.3.0
# source ~/.rvm/scripts/rvm
# rvm use 2.3.0 --default
compass version || gem install compass


echo "+++++++++++++++++++++++++ Running NPM... +++++++++++++++++++++++++"
npm install || ( echo "Error: can't install npm packages:" $? && exit $? )


echo "+++++++++++++++++++++++++ Running BOWER... +++++++++++++++++++++++++"
bower install  || (echo "Error: can't install bower packages:" $? && exit $? )

# echo "copy bower components - it is a hack, we will fix it with new build process for webadmin"
cp -a bower_components app/bower_components/
echo "+++++++++++++++++++++++++ Running GRUNT... +++++++++++++++++++++++++"
cp -f ./target/version_maven.txt ./static/


# echo "create version.txt for the build"
hg log -r "p1()" $SOURCE_PATH > static/version.txt

# echo "run grunt build process"
grunt pub  || ( echo "Error: grunt pub failed" $? && exit $? )

if [ ! -f ./external.dat ]; then echo "Error: no external.dat" && rm ./target/rdep.sh && exit 1; fi
