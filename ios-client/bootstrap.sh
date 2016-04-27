brew update
brew install rbenv ruby-build

grep '^eval "$(rbenv init -)"$' ~/.profile > /dev/null || { echo 'eval "$(rbenv init -)"' >> ~/.profile; eval "$(rbenv init -)"; }
rbenv install 2.0.0-rc2
rbenv local 2.0.0-rc2
gem install cocoapods xcpretty --no-rdoc --no-ri
rbenv rehash
