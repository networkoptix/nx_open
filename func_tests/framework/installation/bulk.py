from contextlib2 import ExitStack, contextmanager


@contextmanager
def many_mediaservers_allocated(machines, mediaserver_allocation):
    with ExitStack() as stack:
        mediaservers = {}
        for alias, machine in machines.items():
            mediaservers[alias] = stack.enter_context(mediaserver_allocation(machine))
            mediaservers[alias].start()
            mediaservers[alias].api.setup_local_system()
        yield mediaservers
