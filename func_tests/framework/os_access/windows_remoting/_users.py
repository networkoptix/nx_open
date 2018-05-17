from pylru import lrudecorator

from ._cim_query import CIMQuery
from ._env_vars import EnvVars


class Users(object):
    def __init__(self, protocol):
        self.protocol = protocol

    @lrudecorator(1)
    def all_profiles(self):
        query = CIMQuery(self.protocol, 'Win32_UserProfile', {})
        profiles = list(query.enumerate())
        return profiles

    @lrudecorator(1)
    def profile_by_sid(self, sid):
        selectors = {'SID': sid}
        query = CIMQuery(self.protocol, 'Win32_UserProfile', selectors)
        profile = query.get_one()
        return profile

    def system_profile(self):
        # See: https://support.microsoft.com/en-us/help/243330
        system_user_sid = 'S-1-5-18'
        return self.profile_by_sid(system_user_sid)

    @lrudecorator(1)
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


