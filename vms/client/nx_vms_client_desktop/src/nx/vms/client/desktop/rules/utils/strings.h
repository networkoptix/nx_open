// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder_fields/action_builder_fields_fwd.h>
#include <nx/vms/rules/event_filter_fields/event_filter_fields_fwd.h>

namespace nx::vms::rules { enum class ResourceType; }

namespace nx::vms::client::desktop::rules {

class Strings
{
    Q_DECLARE_TR_FUNCTIONS(Strings)

public:
    static QString selectButtonText(SystemContext* context, vms::rules::TargetLayoutsField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::TargetDevicesField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::TargetLayoutField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::TargetServersField* field);
    static QString selectButtonText(
        SystemContext* context, vms::rules::TargetDeviceField* field);
    static QString selectButtonText(
        SystemContext* context, vms::rules::TargetUsersField* field, int additionalCount);

    static QString selectButtonText(SystemContext* context, vms::rules::SourceCameraField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::SourceServerField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::SourceUserField* field);

    static QString eventLevelDisplayString(api::EventLevel eventLevel);
    static QString autoValue();
    static QString selectString();
    static QString sourceCameraString();
    static QString devModeInfoTitle();

    static QString isListed();
    static QString isNotListed();
    static QString in();
    static QString allUsers();

    static QString targetRecipientsString(
        const QnUserResourceList& users,
        const api::UserGroupDataList& groups,
        const QStringList& additionalRecipients);

    static QString subjectsShort(
        bool all, const QnUserResourceList& users, const api::UserGroupDataList& groups, int removed);

    static QStringList subjectsLong(
        bool all,
        const QnUserResourceList& users,
        const api::UserGroupDataList& groups,
        int removed,
        int max);

    static QStringList resourcesShort(
        SystemContext* context,
        nx::vms::rules::ResourceType type,
        const QnResourceList& resources,
        int removed);

    static QStringList resourcesLong(
        SystemContext* context,
        nx::vms::rules::ResourceType type,
        const QnResourceList& resources,
        int removed,
        int max);

private:
    static QString getName(SystemContext* context, const QnVirtualCameraResourceList& resources);
    static QString getName(const QnMediaServerResourceList& resources);
    static QString getName(
        const QnUserResourceList& users, const api::UserGroupDataList& groups, int additionalCount);
    static QString getName(
        const QnUserResourceList& users, const api::UserGroupDataList& groups, bool tryNames = true);

    static QString removed(SystemContext* context, nx::vms::rules::ResourceType type, int count);
    static QString number(nx::vms::rules::ResourceType type, const QnResourceList& resources);
    static QString more(SystemContext* context, nx::vms::rules::ResourceType type, int count);
};

} // namespace nx::vms::client::desktop::rules
