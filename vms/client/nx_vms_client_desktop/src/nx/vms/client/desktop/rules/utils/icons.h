// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QIcon>
#include <QtGui/QValidator>

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder_fields/layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_server_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>

namespace nx::vms::client::desktop::rules {

QIcon attentionIcon();

QIcon selectButtonIcon(SystemContext* context, vms::rules::LayoutField* field);
QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetDeviceField* field);
QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetLayoutField* field);
QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetServerField* field);
QIcon selectButtonIcon(SystemContext* context, vms::rules::TargetSingleDeviceField* field);
QIcon selectButtonIcon(
    SystemContext* context, vms::rules::TargetUserField* field, int additionalCount);

QIcon selectButtonIcon(SystemContext* context, vms::rules::SourceCameraField* field);
QIcon selectButtonIcon(SystemContext* context, vms::rules::SourceServerField* field);
QIcon selectButtonIcon(SystemContext* context, vms::rules::SourceUserField* field);

template<class T>
QIcon selectButtonIcon(SystemContext* context, T* field, QValidator::State fieldValidity)
{
    if (fieldValidity == QValidator::State::Intermediate)
        return attentionIcon();

    return selectButtonIcon(context, field);
}

} // namespace nx::vms::client::desktop::rules
