// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "startup_parameters.h"

#include <client/client_startup_parameters.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

StartupParameters StartupParameters::fromCommandLineParams(const QnStartupParameters& params)
{
    StartupParameters result; //< Mode::cleanStartup by default.

    if (!params.sessionId.isEmpty() && !params.stateFileName.isEmpty())
    {
        result.mode = Mode::loadSession;
        result.sessionId = SessionId::fromString(params.sessionId.toStdString());
        result.key = params.stateFileName;
        result.logonData = params.parseAuthenticationString();
    }
    else if (!params.stateFileName.isEmpty())
    {
        result.mode = Mode::inheritState;
        result.key = params.stateFileName;
        result.logonData = params.parseAuthenticationString();
    }

    result.noFullScreen = params.fullScreenDisabled;
    result.screen = params.screen;
    result.windowGeometry = params.windowGeometry;

    return result;
}

QStringList StartupParameters::toCommandLineParams() const
{
    QStringList result;

    switch (mode)
    {
        case Mode::cleanStartup:
        {
            // Default mode. No parameters should be passed.
            break;
        }
        case Mode::inheritState:
        {
            result << QnStartupParameters::kStateFileKey;
            result << key;

            if (!logonData.address.isNull())
            {
                result << QnStartupParameters::kAuthenticationStringKey;
                result << QnStartupParameters::createAuthenticationString(logonData);
            }
            if (!instantDrop.isEmpty())
            {
                result << "--instant-drop";
                result << instantDrop;
            }
            if (!delayedDrop.isEmpty())
            {
                result << "--delayed-drop";
                result << delayedDrop;
            }
            break;
        }
        case Mode::loadSession:
        {
            // Session id and file name should be passed.
            result << QnStartupParameters::kSessionIdKey;
            result << sessionId.toQString();
            result << QnStartupParameters::kStateFileKey;
            result << key;
            result << QnStartupParameters::kAuthenticationStringKey;
            result << QnStartupParameters::createAuthenticationString(logonData);
            break;
        }
        default:
            NX_ASSERT(false, "Unreachable");
            break;
    }

    return result;
}

} // namespace nx::vms::client::desktop
