// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <client/client_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

/**
Workbench pane appearance
*/
struct QnPaneSettings
{
    Qn::PaneState state;    /**< pane state */
    qreal span;             /**< horizontal pane height or vertical pane width, if applicable */

    QnPaneSettings(
        Qn::PaneState paneState = Qn::PaneState::Opened,
        qreal paneSpan = 0.0)
        :
        state(paneState),
        span(paneSpan)
    {
    }

    bool operator==(const QnPaneSettings& other) const;
};

#define QnPaneSettings_Fields (state)(span)
QN_FUSION_DECLARE_FUNCTIONS(QnPaneSettings, (json));
NX_REFLECTION_INSTRUMENT(QnPaneSettings, QnPaneSettings_Fields)

/**
Workbench pane appearances map
*/
typedef QMap<Qn::WorkbenchPane, QnPaneSettings> QnPaneSettingsMap;

namespace Qn
{
    /* Default workbench pane settings */
    const QnPaneSettingsMap& defaultPaneSettings();
};
