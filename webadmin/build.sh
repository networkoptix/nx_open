#!/bin/bash
#rvm install ruby-2.3.0
rvm use 2.3.0 --default

export PATH=$PATH:~/.rvm/bin:~/.rvm/rubies/ruby-2.3.0/bin:~/.rvm/gems/ruby-2.3.0/bin
#sleep 2
#echo $PATH
#sleep 2
echo "+++++++++++++++++++++++++ Running NPM... +++++++++++++++++++++++++"
#gem install compass
#sleep 2
npm install
if [ $? -gt 0 ]; then 
   echo "+++++++++++++++++++++++++ NPM errorlevel:" $? "+++++++++++++++++++++++++"
   exit $?
fi
#sleep 2
echo "+++++++++++++++++++++++++ Running BOWER... +++++++++++++++++++++++++"
bower install
if [ $? -gt 0 ]; then
   echo "+++++++++++++++++++++++++ BOWER errorlevel:" $? "+++++++++++++++++++++++++"
   exit $?
fi
#sleep 2
echo "+++++++++++++++++++++++++ Running GRUNT... +++++++++++++++++++++++++"
cp -f ./target/version_maven.txt ./static/
grunt pub
if [ $? -gt 0 ]; then
   echo "+++++++++++++++++++++++++ GRUNT errorlevel:" $? "+++++++++++++++++++++++++"
   exit $?
fi