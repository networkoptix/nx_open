"""
This file was generated with the custommenu management command, it contains
the classes for the admin menu, you can customize this class as you want.

To activate your custom menu add the following to your settings.py::
    ADMIN_TOOLS_MENU = 'cms.menu.CustomMenu'
"""

try:
    from django.urls import reverse
except ImportError:
    from django.core.urlresolvers import reverse
from django.utils.translation import ugettext_lazy as _

from admin_tools.menu import items, Menu


class CustomMenu(Menu):
    """
    Custom Menu for cms admin site.
    """
    def __init__(self, **kwargs):
        Menu.__init__(self, **kwargs)
        self.children += [
            items.MenuItem(_('Dashboard'), reverse('admin:index')),
            items.Bookmarks(),
            items.AppList(
                _('Applications'),
                exclude=(
                         'cms.models.ContextTemplate',
                         'cms.models.DataRecord',
                         'cms.models.DataStructure',
                         'cms.models.ProductType',
                         'cms.models.ContentVersion',
                         'django_celery_results.*',
                         'notifications.models.*',
                         'rest_hooks.*',
                         'zapier.models.*'
                         )
            ),
            items.AppList(
                _('Internal'),
                models=(
                        'cms.models.ContextTemplate',
                        'cms.models.DataRecord',
                        'cms.models.DataStructure',
                        'cms.models.ProductType',
                        'cms.models.ContentVersion',
                        'django_celery_results.*',
                        'notifications.models.*',
                        'rest_hooks.*',
                        'zapier.models.*'
                        )
            ),
            items.MenuItem('Help', '/static/help/cms/'),
        ]

    def init_with_context(self, context):
        """
        Use this method if you need to access the request context.
        """
        return super(CustomMenu, self).init_with_context(context)
