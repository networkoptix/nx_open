// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "strings.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/action_builder_fields/layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_server_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
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
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop::rules {

QString Strings::selectButtonText(SystemContext* context, vms::rules::LayoutField* field)
{
    if (field->value().isNull())
        return selectString();

    const auto resource = context->resourcePool()->getResourceById<QnLayoutResource>(field->value());
    if (!resource)
        return selectString();

    return resource->getName();
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetDeviceField* field)
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

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetLayoutField* field)
{
    const auto resources =
        context->resourcePool()->getResourcesByIds<QnLayoutResource>(field->value());

    if (resources.empty())
        return selectString();

    if (resources.size() == 1)
        return resources.first()->getName();

    return tr("%n Layouts", "", static_cast<int>(resources.size()));
}

QString Strings::selectButtonText(SystemContext* context, vms::rules::TargetServerField* field)
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
    SystemContext* context, vms::rules::TargetSingleDeviceField* field)
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
    SystemContext* context, vms::rules::TargetUserField* field, int additionalCount)
{
    if (field->acceptAll())
        return tr("All Users");

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

    return tr("%n Servers", "", static_cast<int>(resources.size()));
}

QString Strings::getName(
    const QnUserResourceList& users, api::UserGroupDataList& groups, int additionalCount)
{
    if (users.empty() && groups.empty() && additionalCount == 0)
        return selectString();

    QString additionalName = tr("%n additional", "", additionalCount);
    if (users.empty() && groups.empty())
        return additionalName;

    QString baseName;
    if (users.size() == 1 && groups.empty())
    {
        baseName = users.front()->getName();
    }
    else if (users.empty() && groups.size() <= 2)
    {
        QStringList groupNames;
        for (const auto& group: groups)
            groupNames.push_back(group.name);

        groupNames.sort(Qt::CaseInsensitive);

        baseName = groupNames.join(", ");
    }
    else if (groups.empty())
    {
        baseName = tr("%n Users", "", static_cast<int>(users.size()));
    }
    else if (!users.empty())
    {
        baseName = QString{"%1, %2"}
            .arg(tr("%n Groups", "", static_cast<int>(groups.size())))
            .arg(tr("%n Users", "", static_cast<int>(users.size())));
    }
    else
    {
        baseName = tr("%n Groups", "", static_cast<int>(groups.size()));
    }

    if (additionalCount != 0)
        return QString{"%1, %2 additional"}.arg(baseName).arg(additionalCount);

    return baseName;
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

} // namespace nx::vms::client::desktop::rules
