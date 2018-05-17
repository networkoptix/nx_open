from framework.os_access.windows_remoting.dot_net import enumerate_cim_instances, get_cim_instance
from framework.os_access.windows_remoting.dot_net.env_vars import EnvVars


def user_profiles(protocol):
    return list(enumerate_cim_instances(protocol, 'Win32_UserProfile', {}))


def get_user_profile(protocol, sid):
    return get_cim_instance(protocol, 'Win32_UserProfile', {'SID': sid})


def get_system_user_profile(protocol):
    # https://support.microsoft.com/en-us/help/243330/well-known-security-identifiers-in-windows-operating-systems
    system_user_sid = 'S-1-5-18'
    return get_user_profile(protocol, system_user_sid)


def _get_user_account(protocol, local_name):
    all_accounts = list(user_accounts(protocol))
    for account in all_accounts:
        if account[u'LocalAccount'] == u'true':
            if account[u'Name'] == local_name:
                return account
    return RuntimeError("Cannot find user {}", local_name)


def get_user(protocol, local_name):
    account = _get_user_account(protocol, local_name)
    profile = get_user_profile(protocol, account[u'SID'])
    default_user_env_vars = {u'USERPROFILE': profile[u'LocalPath']}
    env_vars = EnvVars.request(protocol, account[u'Caption'], default_user_env_vars)
    return account, profile, env_vars


def user_accounts(protocol):
    return enumerate_cim_instances(protocol, 'Win32_UserAccount', {})
