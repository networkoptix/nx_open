from netaddr import IPNetwork


def merge_system(server_factory, scheme):
    servers = {}
    for remote_alias, local_aliases in scheme.items():
        for local_alias, merge_parameters in local_aliases.items():
            servers[remote_alias] = server_factory.get(remote_alias)
            servers[local_alias] = server_factory.get(local_alias)
            merge_kwargs = {}
            if merge_parameters is not None:
                try:
                    merge_kwargs['take_remote_settings'] = merge_parameters['settings'] == 'remote'
                except KeyError:
                    pass
                try:
                    remote_network = IPNetwork(merge_parameters['network'])
                except KeyError:
                    pass
                else:
                    merge_kwargs['remote_network'] = remote_network
            servers[local_alias].merge_systems(servers[remote_alias], **merge_kwargs)
    return servers
