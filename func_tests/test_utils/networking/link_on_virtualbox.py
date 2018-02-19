import csv
import subprocess

from netaddr import EUI, IPAddress


class _VirtualboxInfoCSVDialect(csv.Dialect):
    """Dialect guessed by smart csv.Sniffer"""
    delimiter = '='
    quotechar = '"'
    escapechar = '\\'
    doublequote = False  # Quote in VM name is escaped with backslash.
    skipinitialspace = False
    lineterminator = '\n'
    quoting = csv.QUOTE_MINIMAL


_HOST_SLOT = 1
_OTHER_SLOTS = list(range(2, 8 + 1))


def _properties(vm_name):
    command = ['VBoxManage', 'showvminfo', vm_name, '--machinereadable']
    output = subprocess.check_output(command)
    return dict(csv.reader(output.splitlines(), dialect=_VirtualboxInfoCSVDialect()))


def _find_free_adapter(vm_name):
    properties = _properties(vm_name)
    for index in _OTHER_SLOTS:
        if properties['nic{}'.format(index)] == 'null':
            mac_address_key = 'macaddress{}'.format(index)
            return index, EUI(properties[mac_address_key])
    raise Exception("All adapters are busy on %r" % vm_name)


def unplug_all(vm_name):
    for index in range(2, 9):
        key = 'nic{}'.format(index)
        subprocess.check_call(['VBoxManage', 'controlvm', vm_name, key, 'null'])


def plug(vm_name, internal_network_name):
    index, mac_address = _find_free_adapter(vm_name)
    key = 'nic{}'.format(index)
    subprocess.check_call(['VBoxManage', 'controlvm', vm_name, key, 'intnet', internal_network_name])
    return mac_address


def _get_mac_addresses(vm_name, slots):
    properties = _properties(vm_name)
    for slot in slots:
        key = 'macaddress{}'.format(slot)
        as_str = properties[key]
        yield EUI(as_str)


def host_nat_interface_mac_address(vm_name):
    result, = _get_mac_addresses(vm_name, [_HOST_SLOT])
    return result


def other_mac_addresses(vm_name):
    return list(_get_mac_addresses(vm_name, _OTHER_SLOTS))


def host_ip_address():
    """IP address of virtual adapter connecting VMs to internet

    See: https://www.virtualbox.org/manual/ch09.html#changenat.
    Excerpt:
    If, for any reason, the NAT network needs to be changed,
    this can be achieved with the following command:
    VBoxManage modifyvm "VM name" --natnet1 "192.168/16"
    This command would reserve the network addresses
    from 192.168.0.0 to 192.168.254.254 for
    the first NAT network instance of "VM name".
    The guest IP would be assigned to 192.168.0.15 and
    the default gateway could be found at 192.168.0.2.
    """
    return IPAddress('10.10.0.2')
