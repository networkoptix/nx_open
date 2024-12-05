// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "strings.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_layouts_field.h>
#include <nx/vms/rules/action_builder_fields/target_servers_field.h>
#include <nx/vms/rules/action_builder_fields/target_users_field.h>
#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/actions/device_output_action.h>
#include <nx/vms/rules/actions/device_recording_action.h>
#include <nx/vms/rules/actions/enter_fullscreen_action.h>
#include <nx/vms/rules/actions/play_sound_action.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/actions/text_overlay_action.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop::rules {

using nx::vms::rules::ResourceType;

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetLayoutField* field)
{
    if (field->value().isNull())
        return selectString();

    const auto resource = context->resourcePool()->getResourceById<QnLayoutResource>(field->value());
    if (!resource)
        return selectString();

    return resource->getName();
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetDevicesField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(field->ids());

    if (!field->useSource() && resources.empty())
        return selectString();

    if (field->useSource())
    {
        if (resources.empty())
            return sourceCameraString();

        return tr("Source and %n more Cameras", "", static_cast<int>(resources.size()));
    }

    return getName(context, resources);
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetLayoutsField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnLayoutResource>(field->value());

    if (resources.empty())
        return selectString();

    if (resources.size() == 1)
        return resources.first()->getName();

    return number(ResourceType::layout, resources);
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetServersField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnMediaServerResource>(field->ids());

    if (!field->useSource() && resources.empty())
        return selectString();

    if (field->useSource())
    {
        if (resources.empty())
            return tr("Source Server");

        return tr("Source Server and %n Servers", "", static_cast<int>(resources.size()));
    }

    return getName(resources);
}

QString Strings::selectButtonText(
    SystemContext* context, vms::rules::TargetDeviceField* field)
{
    if (field->useSource())
        return sourceCameraString();

    if (field->id().isNull())
        return selectString();

    const auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(field->id());
    if (!camera)
        return selectString();

    return getName(context, {camera});
}

QString Strings::selectButtonText(
    SystemContext* context, vms::rules::TargetUsersField* field, int additionalCount)
{
    if (field->acceptAll())
        return allUsers();

    QnUserResourceList users;
    api::UserGroupDataList groups;
    common::getUsersAndGroups(context, field->ids(), users, groups);

    return getName(users, groups, additionalCount);
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::SourceCameraField* field)
{
    if (field->acceptAll())
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            context->resourcePool(),
            tr("Any Device"),
            tr("Any Camera"));
    }

    const auto resources =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(field->ids());

    if (resources.empty())
        return selectString();

    return getName(context, resources);
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::SourceServerField* field)
{
    const auto properties = field->properties();
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnMediaServerResource>(field->ids());

    if (!properties.allowEmptySelection && resources.empty())
        return selectString();

    if (field->acceptAll())
        return tr("Any Server");

    return getName(resources);
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::SourceUserField* field)
{
    if (field->acceptAll())
        return tr("Any User");

    QnUserResourceList users;
    api::UserGroupDataList groups;
    common::getUsersAndGroups(context, field->ids(), users, groups);

    if (users.empty() && groups.empty())
        return selectString();

    return getName(users, groups);
}

QString Strings::eventLevelDisplayString(api::EventLevel eventLevel)
{
    switch (eventLevel)
    {
        case api::EventLevel::error:
            return tr("Error");
        case api::EventLevel::warning:
            return tr("Warning");
        case api::EventLevel::info:
            return tr("Info");
        default:
            NX_ASSERT(false, "Unexpected api::EventLevel enum value");
            return tr("Undefined");
    }
}

QString Strings::autoValue()
{
    return tr("Auto");
}

QString Strings::getName(SystemContext* context, const QnVirtualCameraResourceList& resources)
{
    if (resources.size() == 1)
        return QnResourceDisplayInfo(resources.first()).toString(Qn::RI_NameOnly);

    return QnDeviceDependentStrings::getNumericName(context->resourcePool(), resources);
}

QString Strings::getName(const QnMediaServerResourceList& resources)
{
    if (resources.size() == 1)
        return resources.first()->getName();

    return number(ResourceType::server, resources);
}

QString Strings::getName(
    const QnUserResourceList& users, const api::UserGroupDataList& groups, int additionalCount)
{
    if (users.empty() && groups.empty() && additionalCount == 0)
        return selectString();

    QString additionalName = tr("%n additional", "", additionalCount);
    if (users.empty() && groups.empty())
        return additionalName;

    auto nameForUsersAndGroups = getName(users, groups);

    if (additionalCount != 0)
        return QString{"%1, %2"}.arg(nameForUsersAndGroups).arg(additionalName);

    return nameForUsersAndGroups;
}

QString Strings::selectString()
{
    return tr("Select");
}

QString Strings::sourceCameraString()
{
    return tr("Source camera");
}

QString Strings::devModeInfoTitle()
{
    return tr("Developer Mode Info");
}

QString Strings::isListed()
{
    return tr("Is listed");
}

QString Strings::isNotListed()
{
    return tr("Is not listed");
}

QString Strings::in()
{
    return tr("In");
}

QString Strings::allUsers()
{
    return tr("All Users");
}

QString Strings::targetRecipientsString(
    const QnUserResourceList& users,
    const api::UserGroupDataList& groups,
    const QStringList& additionalRecipients)
{
    QStringList result = additionalRecipients;

    const auto nameForUsersAndGroups = getName(users, groups);
    if (!nameForUsersAndGroups.isEmpty())
        result.push_front(nameForUsersAndGroups);

    return result.join("; ");
}

QString Strings::subjectsShort(
    bool all, const QnUserResourceList& users, const api::UserGroupDataList& groups, int removed)
{
    if (all)
        return allUsers();

    if (removed <= 0)
        return getName(users, groups, /*tryNames*/ true);

    const QString removedSubjectsText = Strings::removed(nullptr, ResourceType::user, removed);
    return groups.empty() && users.empty()
        ? removedSubjectsText
        : getName(users, groups, /*tryNames*/ false) + ", " + removedSubjectsText;
}

QStringList Strings::subjectsLong(
    bool all,
    const QnUserResourceList& users,
    const api::UserGroupDataList& groups,
    int removed,
    int max)
{
    if (all)
        return QStringList{allUsers()};

    QStringList result;

    for (int i = 0; i < max && i < users.count(); ++i)
        result.push_back(users[i]->getName());

    result.sort(Qt::CaseInsensitive);
    max -= result.size();

    QStringList groupNames;
    for  (int i = 0; i < max && i < groups.size(); ++i)
        groupNames.push_back(groups[i].name);

    groupNames.sort(Qt::CaseInsensitive);
    result.append(std::move(groupNames));

    if (const int more = users.size() + groups.size() - result.size(); more > 0)
        result.push_back(Strings::more(nullptr, ResourceType::user, more));

    if (removed > 0)
        result.push_back(Strings::removed(nullptr, ResourceType::user, removed));

    return result;
}

QStringList Strings::resourcesShort(
    SystemContext* context,
    nx::vms::rules::ResourceType type,
    const QnResourceList& resources,
    int removed)
{
    const int maxNames = (removed > 0) ? 1 : 2;
    const auto infoLevel = appContext()->localSettings()->resourceInfoLevel();
    QStringList result;

    if (resources.size() <= maxNames)
    {
        for (const auto& resource: resources)
            result.push_back(nx::vms::rules::Strings::resource(resource, infoLevel));

        result.sort(Qt::CaseInsensitive);
    }
    else if (!resources.empty())
    {
        result.push_back(number(type, resources));
    }

    if (removed > 0)
        result.push_back(Strings::removed(context, type, removed));

    return result;
}

QStringList Strings::resourcesLong(
    SystemContext* context,
    nx::vms::rules::ResourceType type,
    const QnResourceList& resources,
    int removed,
    int max)
{
    if (resources.empty())
        return {};

    const auto infoLevel = appContext()->localSettings()->resourceInfoLevel();
    QStringList result;

    for (int i = 0; i < max && i < resources.size(); ++i)
        result.push_back(nx::vms::rules::Strings::resource(resources[i], infoLevel));

    result.sort(Qt::CaseInsensitive);

    if (const int more = resources.size() - result.size(); more > 0)
        result.push_back(Strings::more(context, type, more));

    if (removed > 0)
        result.push_back(Strings::removed(context, type, removed));

    return result;
}

QString Strings::getName(
    const QnUserResourceList& users, const api::UserGroupDataList& groups, bool tryNames)
{
    if (users.empty() && groups.empty())
        return {};

    if (tryNames)
    {
        if (users.size() == 1 && groups.empty())
            return users.front()->getName();

        if (users.empty() && groups.size() <= 2)
        {
            QStringList groupNames;
            for (const auto& group: groups)
                groupNames.push_back(group.name);

            groupNames.sort(Qt::CaseInsensitive);

            return groupNames.join(", ");
        }
    }

    QStringList result;

    if (!groups.empty())
        result.push_back(tr("%n Groups", "", static_cast<int>(groups.size())));

    if (!users.empty())
        result.push_back(tr("%n Users", "", static_cast<int>(users.size())));

    return result.join(", ");
}

QString Strings::removed(SystemContext* context, nx::vms::rules::ResourceType type)
{
    switch (type)
    {
        case ResourceType::user:
            return tr("Removed subject", "The subject is user or group");
        case ResourceType::device:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(), tr("Removed device"), tr("Removed camera"));
        case ResourceType::server:
            return tr("Removed server");
        case ResourceType::layout:
            return tr("Removed layout");
        default:
            NX_ASSERT(false, "Unexpected resource type: %1", type);
            return {};
    }
}

QString Strings::removed(SystemContext* context, nx::vms::rules::ResourceType type, int count)
{
    if (!NX_ASSERT(count > 0))
        return {};

    switch (type)
    {
        case ResourceType::user:
            return tr("%n removed subjects", "The subject is user or group", count);
        case ResourceType::device:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(),
                tr("%n removed devices", {}, count),
                tr("%n removed cameras", {}, count));
        case ResourceType::server:
            return tr("%n removed servers", {}, count);
        case ResourceType::layout:
            return tr("%n removed layouts", {}, count);
        default:
            NX_ASSERT(false, "Unexpected resource type: %1", type);
            return {};
    }
}

QString Strings::more(SystemContext* context, nx::vms::rules::ResourceType type, int count)
{
    if (!NX_ASSERT(count > 0))
        return {};

    switch (type)
    {
        case ResourceType::user:
            return tr("%n subjects more", "", count);
        case ResourceType::device:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(),
                tr("%n devices more", {}, count),
                tr("%n cameras more", {}, count));
        case ResourceType::server:
            return tr("%n servers more", {}, count);
        case ResourceType::layout:
            return tr("%n layouts more", {}, count);
        default:
            NX_ASSERT(false, "Unexpected resource type: %1", type);
            return {};
    }
}

QString Strings::number(nx::vms::rules::ResourceType type, const QnResourceList& resources)
{
    if (!NX_ASSERT(!resources.empty()))
        return {};

    switch (type)
    {
        case ResourceType::device:
            return QnDeviceDependentStrings::getNumericName(
                SystemContext::fromResource(resources.first())->resourcePool(),
                resources.filtered<QnVirtualCameraResource>());
        case ResourceType::server:
            return tr("%n Servers", {}, resources.size());
        case ResourceType::layout:
            return tr("%n Layouts", {}, resources.size());
        default:
            NX_ASSERT(false, "Unexpected resource type: %1", type);
            return {};
    }
}

} // namespace nx::vms::client::desktop::rules
