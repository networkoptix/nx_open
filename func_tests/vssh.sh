#!/bin/bash
key=~/.vagrant.d/insecure_private_key
box="$1"
shift
if [ -z "$box" ]; then
	echo No box addess provided!
	exit 1
fi
ssh "vagrant@$box" -o Compression=yes -o DSAAuthentication=yes -o LogLevel=ERROR -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o IdentitiesOnly=yes -i "$key" -o ForwardAgent=yes "$@"
