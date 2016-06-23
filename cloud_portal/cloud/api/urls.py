__author__ = 'noptix'

from django.conf.urls import url
from api.views import account, systems, common

urlpatterns = [
    url(r'^account/activate',           account.activate),
    url(r'^account/login',              account.login),
    url(r'^account/logout',             account.logout),
    url(r'^account/register',           account.register),
    url(r'^account/restorePassword',    account.restore_password),
    url(r'^account/changePassword',     account.change_password),
    url(r'^account/authKey',            account.auth_key),
    url(r'^account/?$',                 account.index),


    url(r'^systems/disconnect',                     systems.disconnect),
    url(r'^systems/connect',                        systems.connect),
    url(r'^systems/(?P<system_id>.+?)/accessRoles', systems.access_roles),
    url(r'^systems/(?P<system_id>.+?)/users',       systems.sharing),
    url(r'^systems/(?P<system_id>.+?)/proxy/(?P<system_url>.+?)$',         systems.proxy),
    url(r'^systems/(?P<system_id>.+?)/?$',          systems.system),
    url(r'^systems/?$',                             systems.list_systems),


    url(r'^ping',                                   common.ping),
    url(r'^cloud_modules',                          common.cloud_modules),
]
