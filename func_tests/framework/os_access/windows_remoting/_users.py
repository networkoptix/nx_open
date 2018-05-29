from framework.method_caching import cached_getter
from ._cim_query import CIMQuery


class Users(object):
    def __init__(self, protocol):
        # TODO: Work via WinRM in this class.
        self.protocol = protocol

    @cached_getter
    def all_profiles(self):
        query = CIMQuery(self.protocol, 'Win32_UserProfile', {})
        profiles = list(query.enumerate())
        return profiles

    def profile_by_sid(self, sid):
        selectors = {'SID': sid}
        query = CIMQuery(self.protocol, 'Win32_UserProfile', selectors)
        profile = query.get_one()
        return profile

    @cached_getter
    def system_profile(self):
        # See: https://support.microsoft.com/en-us/help/243330
        system_user_sid = 'S-1-5-18'
        return self.profile_by_sid(system_user_sid)

    @cached_getter
    def all_accounts(self):
        query = CIMQuery(self.protocol, 'Win32_UserAccount', {})
        accounts = list(query.enumerate())
        return accounts

    def account_by_name(self, local_name):
        all_accounts = self.all_accounts()
        for account in all_accounts:
            if account[u'LocalAccount'] == u'true':
                if account[u'Name'] == local_name:
                    return account
        return RuntimeError("Cannot find user {}", local_name)
