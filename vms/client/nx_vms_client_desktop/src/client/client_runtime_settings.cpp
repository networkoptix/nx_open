// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_runtime_settings.h"

#include <nx/utils/log/log.h>

#include "client_settings.h"
#include "client_startup_parameters.h"

static QnClientRuntimeSettings* s_instance = nullptr;

QnClientRuntimeSettings::QnClientRuntimeSettings(
    const QnStartupParameters& startupParameters,
    QObject* parent)
    :
    base_type(parent)
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

    init();
    setThreadSafe(true);

    setSoftwareYuv(startupParameters.softwareYuv);
    setShowFullInfo(startupParameters.showFullInfo);

    setClientUpdateAllowed(!startupParameters.clientUpdateDisabled);

    // TODO: #sivanov Implement lexical serialization.
    if (!startupParameters.lightMode.isEmpty())
    {
        bool ok = false;
        const Qn::LightModeFlags lightModeOverride(startupParameters.lightMode.toInt(&ok));
        setLightModeOverride(ok ? lightModeOverride : Qn::LightModeFull);
    }

    if (startupParameters.isVideoWallMode())
    {
        setVideoWallMode(true);
        setLightModeOverride(Qn::LightModeVideoWall);
    }

    if (startupParameters.acsMode)
    {
        setAcsMode(true);
        setLightModeOverride(Qn::LightModeACS);
    }

    // Default value.
    if (lightModeOverride() != -1)
        setLightMode(static_cast<Qn::LightModeFlags>(lightModeOverride()));
}

QnClientRuntimeSettings::~QnClientRuntimeSettings()
{
    if (s_instance == this)
        s_instance = nullptr;
}

QnClientRuntimeSettings* QnClientRuntimeSettings::instance()
{
    return s_instance;
}

bool QnClientRuntimeSettings::isDesktopMode() const
{
    return !isVideoWallMode() && !isAcsMode();
}

int QnClientRuntimeSettings::maxSceneItems() const
{
    if (maxSceneItemsOverride() > 0)
        return maxSceneItemsOverride();

    return lightMode().testFlag(Qn::LightModeSingleItem)
        ? 1
        : qnSettings->maxSceneVideoItems();
}
