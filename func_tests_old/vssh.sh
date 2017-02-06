#!/bin/bash
#key=~/.vagrant.d/insecure_private_key
box="$1"
shift
if [ -z "$box" ]; then
	echo No box name provided!
	exit 1
fi
dir=`dirname $0`
conf="$dir/ssh.${box}.conf"
if [ ! -e "$conf" ]; then
    echo "No configuration file $conf found!"
    exit 1
fi
name=`grep ^Host "$conf"|awk '{print $2}'`
ssh -F $conf $name "$@"
#ssh "vagrant@$box" -o Compression=yes -o DSAAuthentication=yes -o LogLevel=ERROR -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o IdentitiesOnly=yes -i "$key" -o ForwardAgent=yes "$@"
