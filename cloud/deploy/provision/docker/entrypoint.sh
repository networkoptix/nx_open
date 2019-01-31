#!/bin/sh

if [[ ! -d /hostssh ]]; then
    echo "Must mount the host SSH directory at /hostssh, e.g. 'docker run --net host -v /root/.ssh:/hostssh nathanleclaire/ansible"
    exit 1
fi

if [ -n "$MODULE_CONFIGURATION" ]
then
    echo "$MODULE_CONFIGURATION" | /usr/local/bin/merge_config.py /playbooks/vars/telegraf.yml
fi

# Generate temporary SSH key to allow access to the host machine.
mkdir -p /root/.ssh
ssh-keygen -f /root/.ssh/id_rsa -P ""

cp /hostssh/authorized_keys /hostssh/authorized_keys.bak
cat /root/.ssh/id_rsa.pub >>/hostssh/authorized_keys

ansible all -i "localhost," -m raw -a "apt-get install -y python-minimal"
ansible-playbook -i "localhost," "$@"

mv /hostssh/authorized_keys.bak /hostssh/authorized_keys
