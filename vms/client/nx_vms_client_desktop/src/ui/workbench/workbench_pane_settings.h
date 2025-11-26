// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <client/client_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

/** Workbench pane appearance */
// FIXME: Refactor pinned, opened & expanded to traditional flags.
struct QnPaneSettings
{
    /** Pane state. */
    Qn::PaneState state = Qn::PaneState::Opened;

    /** Horizontal pane height or vertical pane width, if applicable. */
    qreal span = 0.0;

    /** Whether pane is expanded or collapsed, if applicable. */
    bool expanded = false;

    bool operator==(const QnPaneSettings& other) const = default;
};

#define QnPaneSettings_Fields (state)(span)(expanded)
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
