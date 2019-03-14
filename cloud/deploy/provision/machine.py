#!/usr/bin/env python

"""
Example Usage:
$ ansible -i machine.py machinename -m ping
"""


import argparse
import subprocess

try:
    import json
except ImportError:
    import simplejson as json

def dm(*args):
    return subprocess.check_output(["docker-machine"] + list(args)).strip()

def dminspect(fmt, mcn):
    return dm("inspect", "-f", fmt, mcn)

def get_host_and_vars(m):
    hosts = [dminspect("{{.Driver.IPAddress}}", m)]
    ssh_vars = {
        "ansible_ssh_user": dminspect("{{.Driver.SSHUser}}", m),
        "ansible_ssh_port": dminspect("{{.Driver.SSHPort}}", m),
        "ansible_ssh_private_key_file": dminspect("{{.Driver.SSHKeyPath}}", m)
    }
    data = {"hosts": hosts, "vars": ssh_vars}
    return data

class DockerMachineInventory(object):
    def __init__(self):
        self.inventory = {} # Ansible Inventory

        parser = argparse.ArgumentParser(description='Produce an Ansible Inventory file based on Docker Machine status')
        parser.add_argument('--list', action='store_true', help='List all active Droplets as Ansible inventory (default: True)')
        self.args = parser.parse_args()

        machines = dm("ls", "-q").splitlines()
        json_data = {m: get_host_and_vars(m) for m in machines}

        print json.dumps(json_data)

DockerMachineInventory()
