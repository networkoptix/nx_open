__author__ = 'noptix'

from django.conf.urls import url
from api.methods import account

urlpatterns = [
    url(r'^account$',                   account.index),
    url(r'^account/login',              account.login),
    url(r'^account/logout',             account.logout),
    url(r'^account/register',           account.register),
    url(r'^account/activate',           account.activate),
    url(r'^account/restorePassword',    account.restorePassword),
    url(r'^account/changePassword',     account.changePassword)
]
