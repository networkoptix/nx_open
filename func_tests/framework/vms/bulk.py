from contextlib2 import ExitStack, contextmanager
from typing import Mapping, Sequence

from framework.utils import parse_size
from framework.vms.vm_type import VMType, VM


@contextmanager
def many_allocated(machine_types, machine_configurations):
    # type: (Mapping[str, VMType], Sequence[Mapping[str, ...]]) -> ...
    with ExitStack() as stack:
        machines = {}
        for conf in machine_configurations:
            machine_type = machine_types[conf['type']]
            machine = stack.enter_context(machine_type.vm_ready(conf['alias']))  # type: VM
            machines[conf['alias']] = machine
            if 'free_disk_space' in conf:
                free_space_bytes = parse_size(conf['free_disk_space'])
                free_space_cm = machine.os_access.free_disk_space_limited(free_space_bytes)
                stack.enter_context(free_space_cm)
        yield machines
