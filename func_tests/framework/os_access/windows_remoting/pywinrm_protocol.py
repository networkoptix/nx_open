from winrm import Protocol


def make_pywinrm_protocol(address, port, username, password):
    return Protocol(
        'http://{}:{}/wsman'.format(address, port),
        username=username, password=password,
        transport='ntlm',
        # 'plaintext' is easier to debug but, for some obscure reason, is slower.
        message_encryption='never',  # Any value except 'always' and 'auto'.
        operation_timeout_sec=120, read_timeout_sec=240)
