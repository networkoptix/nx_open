from django.contrib.auth.models import Group, Permission
from cms.models import UserGroupsToProductPermissions


def make_customization_visible_to_user(cloud_portal, user):
    customization_groups = UserGroupsToProductPermissions.objects.\
        filter(product=cloud_portal, group__permissions__codename='can_view_customization')

    if customization_groups.exists():
        customization_groups.first().group.user_set.add(user)
    else:
        group_name = "Can View {}".format(cloud_portal.customizations.first().name)
        can_view_permission = Permission.objects.get(codename='can_view_permission')
        can_view_group = Group(name=group_name)
        can_view_group.save()
        can_view_group.user_set.add(user)
        can_view_group.permissions.add(can_view_permission)
        UserGroupsToProductPermissions(product=cloud_portal, group=can_view_group).save()