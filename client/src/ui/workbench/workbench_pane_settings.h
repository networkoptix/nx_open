#pragma once

#include <QtCore/QMap>
#include <client/client_globals.h>
#include <utils/common/model_functions_fwd.h>

/**
Workbench pane appearance
*/
struct QnPaneSettings
{
    Qn::PaneState state;    /**< pane state */
    qreal span;             /**< horizontal pane height or vertical pane width, if applicable */

    QnPaneSettings() : state(Qn::PaneState::Opened), span(0.0) {}
};

#define QnPaneSettings_Fields (state)(span)
QN_FUSION_DECLARE_FUNCTIONS(QnPaneSettings, (json)(metatype)(eq));

/**
Workbench pane appearances map
*/
typedef QMap<Qn::WorkbenchPane, QnPaneSettings> QnPaneSettingsMap;
