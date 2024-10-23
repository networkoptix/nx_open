// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>
#include <QtCore/QString>

#include <nx/vms/client/core/network/logon_data.h>

#include "session_id.h"

struct QnStartupParameters;

namespace nx::vms::client::desktop {

struct StartupParameters
{
    enum class Mode
    {
        cleanStartup,
        inheritState,
        loadSession,
    };

    Mode mode = Mode::cleanStartup;
    SessionId sessionId;
    QString key;
    core::LogonData logonData;

    QString instantDrop;
    QString delayedDrop;

    bool noFullScreen = false;
    int screen = -1; //< Negative, since 0 is a valid screen index.
    QRect windowGeometry;

    static StartupParameters fromCommandLineParams(const QnStartupParameters& params);
    QStringList toCommandLineParams() const;
    bool allowMultipleClientInstances = false;
};

} // namespace nx::vms::client::desktop
