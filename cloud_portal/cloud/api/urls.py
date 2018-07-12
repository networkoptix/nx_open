__author__ = 'noptix'

from django.conf.urls import url
from api.views import account, systems, common, utils

urlpatterns = [
    url(r'^account-autocomplete/$', account.AccountAutocomplete.as_view(), name='account-autocomplete',),
    url(r'^utils/visitedKey/?$',                utils.visited_key),
    url(r'^utils/language/?$',                  utils.language),
    url(r'^utils/downloads/history$',           utils.downloads_history),
    url(r'^utils/downloads/(?P<build>.+?)$',    utils.download_build),
    url(r'^utils/downloads/?$',                 utils.downloads),
    url(r'^utils/settings/?$',                  utils.settings),


    url(r'^account/activate$',           account.activate),
    url(r'^account/login$',              account.login),
    url(r'^account/logout$',             account.logout),
    url(r'^account/register$',           account.register),
    url(r'^account/restorePassword$',    account.restore_password),
    url(r'^account/changePassword$',     account.change_password),
    url(r'^account/authKey$',            account.auth_key),
    url(r'^account/checkCode$',          account.check_code_in_portal),
    url(r'^account/?$',                  account.index),


    url(r'^systems/disconnect$',                     systems.disconnect),
    url(r'^systems/connect$',                        systems.connect),
    url(r'^systems/merge$',                          systems.merge),
    url(r'^systems/(?P<system_id>.+?)/accessRoles$', systems.access_roles),
    url(r'^systems/(?P<system_id>.+?)/auth$',        systems.get_auth),
    url(r'^systems/(?P<system_id>.+?)/name$',        systems.rename),
    url(r'^systems/(?P<system_id>.+?)/users$',       systems.sharing),
    url(r'^systems/(?P<system_id>.+?)/proxy/(?P<system_url>.+?)$',         systems.proxy),
    url(r'^systems/(?P<system_id>.+?)/?$',           systems.system),
    url(r'^systems/?$',                              systems.list_systems),


    url(r'^ping$',                                   common.ping),
]
