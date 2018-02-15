def _patch_sshd_config(root_os_access, new_settings, config_path='/etc/ssh/sshd_config'):
    current_lines = root_os_access.read_file(config_path).splitlines()  # Preserve new line at the end!
    updated_lines = list()
    presented_keys = set()
    for line in current_lines:
        if line and not line.startswith('#'):
            key, current_value = line.split(' ', 1)
            if key in new_settings:
                presented_keys.add(key)
                if current_value != new_settings[key]:
                    updated_lines.append(key + ' ' + new_settings[key])
                    continue
        updated_lines.append(line)
    additional_lines = []
    for key, value in new_settings.items():
        if key not in presented_keys:
            additional_lines.append(key + ' ' + new_settings[key])
    if additional_lines:
        updated_lines.append("")
        updated_lines.append("# Added by func_tests framework")
        updated_lines.extend(additional_lines)
    if current_lines != updated_lines:
        root_os_access.write_file(config_path, '\n'.join(updated_lines) + '\n')
        root_os_access.run_command(['service', 'ssh', 'reload'])


def optimize_sshd(root_os_access):
    """Used to update configuration to make connection faster not to involve internet

    With default settings, connection may take a while.
    See: https://unix.stackexchange.com/a/298797/152472.
    """
    _patch_sshd_config(root_os_access, {'UsePAM': 'no', 'UseDNS': 'no'})
