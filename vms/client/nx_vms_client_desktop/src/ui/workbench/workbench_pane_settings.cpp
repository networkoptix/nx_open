#include "workbench_pane_settings.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPaneSettings, (json)(eq), QnPaneSettings_Fields)

namespace Qn
{
    const QnPaneSettingsMap& defaultPaneSettings()
    {
        static const QnPaneSettingsMap s_map = {
            { Qn::WorkbenchPane::Title,         QnPaneSettings(Qn::PaneState::Opened) },
            { Qn::WorkbenchPane::Tree,          QnPaneSettings(Qn::PaneState::Opened, 250) },
            { Qn::WorkbenchPane::Notifications, QnPaneSettings(Qn::PaneState::Opened) },
            { Qn::WorkbenchPane::Navigation,    QnPaneSettings(Qn::PaneState::Unpinned) },
            { Qn::WorkbenchPane::Calendar,      QnPaneSettings(Qn::PaneState::Unpinned) },
            { Qn::WorkbenchPane::Thumbnails,    QnPaneSettings(Qn::PaneState::Closed, 48) } };

        return s_map;
    }
}
