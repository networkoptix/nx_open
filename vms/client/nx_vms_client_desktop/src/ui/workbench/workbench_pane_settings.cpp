// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_pane_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_loading_manager.h>

using nx::vms::client::desktop::workbench::timeline::ThumbnailLoadingManager;

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPaneSettings, (json), QnPaneSettings_Fields)

bool QnPaneSettings::operator==(const QnPaneSettings& other) const
{
    return nx::reflect::equals(*this, other);
}

namespace Qn
{
    const QnPaneSettingsMap& defaultPaneSettings()
    {
        static const QnPaneSettingsMap s_map = {
            { Qn::WorkbenchPane::Title,         QnPaneSettings(Qn::PaneState::Opened) },
            { Qn::WorkbenchPane::Tree,          QnPaneSettings(Qn::PaneState::Opened, 250) },
            { Qn::WorkbenchPane::Notifications, QnPaneSettings(Qn::PaneState::Opened) },
            { Qn::WorkbenchPane::Navigation,    QnPaneSettings(Qn::PaneState::Opened) },
            { Qn::WorkbenchPane::Calendar,      QnPaneSettings(Qn::PaneState::Closed) },
            { Qn::WorkbenchPane::Thumbnails,    QnPaneSettings(Qn::PaneState::Closed,
                ThumbnailLoadingManager::kDefaultSize.height())}};

        return s_map;
    }
}
