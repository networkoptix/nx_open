from framework.method_caching import cached_getter
from framework.os_access.windows_remoting import WinRM


class Users(object):
    def __init__(self, winrm):
        self._winrm = winrm  # type: WinRM

    @cached_getter
    def all_profiles(self):
        query = self._winrm.wmi.cls(u'Win32_UserProfile').enumerate({})
        profiles = list(query.enumerate())
        return profiles

    def profile_by_sid(self, sid):
        return self._winrm.wmi.cls(u'Win32_UserProfile').reference({u'SID': sid}).get()

    @cached_getter
    def system_profile(self):
        # See: https://support.microsoft.com/en-us/help/243330
        system_user_sid = 'S-1-5-18'
        return self.profile_by_sid(system_user_sid)

    @cached_getter
    def all_accounts(self):
        accounts = list(self._winrm.wmi.cls(u'Win32_UserAccount').enumerate({}))
        return accounts

    def account_by_name(self, local_name):
        all_accounts = self.all_accounts()
        for account in all_accounts:
            if account[u'LocalAccount'] == u'true':
                if account[u'Name'] == local_name:
                    return account
        return RuntimeError("Cannot find user {}", local_name)
