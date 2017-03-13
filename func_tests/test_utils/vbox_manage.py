'''Virtual Box VBoxManage command wrapper'''

from .host import Host


class VBoxManage(object):

    def __init__(self, host):
        assert isinstance(host, Host), repr(host)
        self.host = host

    def does_vms_exist(self, vms_name):
        output = self.host.run_command(['VBoxManage', '--nologo', 'list', 'vms'])
        vms_list = [line.split()[0].strip('"') for line in output.splitlines()]
        return vms_name in vms_list

    def get_vms_state(self, vms_name):
        output = self.host.run_command(['VBoxManage', '--nologo', 'showvminfo', vms_name, '--machinereadable'])
        properties = {}
        for line in output.splitlines():
            name, value = line.split('=')
            properties[name] = value.strip('"')
        return properties['VMState']

    def poweroff_vms(self, vms_name):
        self.host.run_command(['VBoxManage', '--nologo', 'controlvm', vms_name, 'poweroff'])

    def delete_vms(self, vms_name):
        self.host.run_command(['VBoxManage', '--nologo', 'unregistervm', '--delete', vms_name])
