from framework.os_access.windows_remoting.dot_net import get_cim_instance, enumerate_cim_instances


def enumerate_user_profiles(protocol):
    return list(enumerate_cim_instances(protocol, 'Win32_UserProfile', {}))


def get_system_user_profile(protocol):
    # https://support.microsoft.com/en-us/help/243330/well-known-security-identifiers-in-windows-operating-systems
    system_user_sid = 'S-1-5-18'
    return get_cim_instance(protocol, 'Win32_UserProfile', {'SID': system_user_sid})
