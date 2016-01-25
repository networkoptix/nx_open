__author__ = 'noptix'

from django.conf.urls import url
from api.views import account, systems

urlpatterns = [
    url(r'^account/activate',           account.activate),
    url(r'^account/login',              account.login),
    url(r'^account/logout',             account.logout),
    url(r'^account/register',           account.register),
    url(r'^account/activate',           account.activate),
    url(r'^account/restorePassword',    account.restore_password),
    url(r'^account/changePassword',     account.change_password),
    url(r'^account',                   account.index),


    url(r'^systems/accessRoles',                    systems.access_roles),
    url(r'^systems/(?P<system_id>.+?)/users',       systems.sharing),
    url(r'^systems',                                systems.list_systems),


]
