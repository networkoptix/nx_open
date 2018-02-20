class VirtualboxManagement(object):

    def __init__(self, os_access):
        self._host_os_access = os_access

    def get_vms_list(self):
        output = self._run_command(['list', 'vms'])
        return [line.split()[0].strip('"') for line in output.splitlines()]

    def get_vms_state(self, vms_name):
        output = self._run_command(['showvminfo', vms_name, '--machinereadable'], log_output=False)
        properties = {}
        for line in output.splitlines():
            name, value = line.split('=')
            properties[name] = value.strip('"')
        return properties['VMState']

    def poweroff_vms(self, vms_name):
        self._run_command(['controlvm', vms_name, 'poweroff'])

    def delete_vms(self, vms_name):
        self._run_command(['unregistervm', '--delete', vms_name])

    def _run_command(self, args, log_output=True):
        return self._host_os_access.run_command(['VBoxManage', '--nologo'] + args, log_output=log_output)
