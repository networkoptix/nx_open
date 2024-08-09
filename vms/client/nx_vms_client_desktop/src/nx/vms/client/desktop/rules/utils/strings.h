// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder_fields/action_builder_fields_fwd.h>
#include <nx/vms/rules/event_filter_fields/event_filter_fields_fwd.h>

namespace nx::vms::client::desktop::rules {

class Strings
{
    Q_DECLARE_TR_FUNCTIONS(Strings)

public:
    static QString selectButtonText(SystemContext* context, vms::rules::LayoutField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::TargetDeviceField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::TargetLayoutField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::TargetServerField* field);
    static QString selectButtonText(
        SystemContext* context, vms::rules::TargetSingleDeviceField* field);
    static QString selectButtonText(
        SystemContext* context, vms::rules::TargetUserField* field, int additionalCount);

    static QString selectButtonText(SystemContext* context, vms::rules::SourceCameraField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::SourceServerField* field);
    static QString selectButtonText(SystemContext* context, vms::rules::SourceUserField* field);

    static QString useSourceCameraString(const QString& actionType = {});
    static QString eventLevelDisplayString(api::EventLevel eventLevel);
    static QString autoValue();
    static QString selectString();
    static QString devModeInfoTitle();

    static QString isListed();
    static QString isNotListed();
    static QString in();

private:
    static QString getName(SystemContext* context, const QnVirtualCameraResourceList& resources);
    static QString getName(const QnMediaServerResourceList& resources);
    static QString getName(
        const QnUserResourceList& users, api::UserGroupDataList& groups, int additionalCount = 0);

    static QString sourceCameraString();
};

} // namespace nx::vms::client::desktop::rules
