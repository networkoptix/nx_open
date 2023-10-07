// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/saveable_system_settings.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class AbstractSystemSettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
public:
    AbstractSystemSettingsWidget(
        api::SaveableSystemSettings* editableSystemSettings,
        QWidget* parent = nullptr)
        :
        QnAbstractPreferencesWidget(parent),
        QnWorkbenchContextAware(parent),
        editableSystemSettings(editableSystemSettings)
    {
    }

protected:
    api::SaveableSystemSettings* const editableSystemSettings;
};

} // namespace nx::vms::client::desktop
