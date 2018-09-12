from contextlib2 import contextmanager, ExitStack
from typing import Mapping, Sequence

from framework.vms.vm_type import VMType


@contextmanager
def many_allocated(machine_types, machine_configurations):
    # type: (Mapping[str, VMType], Sequence[Mapping[str, ...]]) -> ...
    with ExitStack() as stack:
        machines = {}
        for conf in machine_configurations:
            machine_type = machine_types[conf['type']]
            machine = stack.enter_context(machine_type.vm_ready(conf['alias']))
            machines[conf['alias']] = machine
        yield machines
