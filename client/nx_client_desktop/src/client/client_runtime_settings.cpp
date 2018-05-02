#include "client_runtime_settings.h"

#include <client/client_settings.h>

QnClientRuntimeSettings::QnClientRuntimeSettings(QObject *parent):
    base_type(parent)
{
    init();
    setThreadSafe(true);
}

QnClientRuntimeSettings::~QnClientRuntimeSettings()
{

}

bool QnClientRuntimeSettings::isDesktopMode() const
{
    return !isVideoWallMode() && !isActiveXMode();
}

int QnClientRuntimeSettings::maxSceneItems() const
{
    if (maxSceneItemsOverride() > 0)
        return maxSceneItemsOverride();

    return qnSettings->lightMode().testFlag(Qn::LightModeSingleItem)
        ? 1
        : qnSettings->maxSceneVideoItems();
}
