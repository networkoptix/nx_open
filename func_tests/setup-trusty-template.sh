#!/bin/bash
set -euxo pipefail
NAME='trusty'
DIRTY="${NAME}-dirty"  # "Dirty" Machine can be deleted in this script, completed cannot.
COMPLETE="${NAME}-template"
KEY=~/".func_tests/${NAME}-key"
VBoxManage controlvm "${DIRTY}" poweroff || :
sleep 1s
VBoxManage unregistervm "${DIRTY}" --delete || :
sleep 1s
rm -rf .temp_vagrant || :
mkdir .temp_vagrant
cd .temp_vagrant
cat >> Vagrantfile <<VAGRANTFILE
Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/trusty64"
  config.vm.provider "virtualbox" do |v|
    v.name = "${DIRTY}"
  end
end
VAGRANTFILE
vagrant up
mkdir -p "$(dirname "${KEY}")"
find -name private_key | xargs -I '{}' cp '{}' "${KEY}.key"
ssh-keygen -y -f "${KEY}.key" > "${KEY}.pub"
vagrant ssh -c 'cat - >> /home/vagrant/.ssh/authorized_keys' < "${KEY}.pub"
vagrant ssh -c 'sudo cp /home/vagrant/.ssh/authorized_keys /root/.ssh/authorized_keys'
vagrant ssh -c 'sudo chown root:root /root/.ssh/authorized_keys'
cd -
rm -rf .temp_vagrant
VBoxManage controlvm "${DIRTY}" acpipowerbutton
sleep 10s

VBoxManage modifyvm "${DIRTY}" --natpf1 delete "ssh"  # Left by Vagrant.
VBoxManage snapshot "${DIRTY}" take "template"
VBoxManage modifyvm "${DIRTY}" --name "${COMPLETE}"  # If this line is reached, above was successful (set -e).
