#include "client_settings_watcher.h"

#include <client/client_settings.h>
#include <client/client_instance_manager.h>

namespace {

constexpr int kCheckSettingsDelayMs = 5000;

}

QnClientSettingsWatcher::QnClientSettingsWatcher(QObject* parent):
    base_type(parent)
{
    auto timer = new QTimer(this);
    timer->setInterval(kCheckSettingsDelayMs);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this,
        [this]
        {
            if (!qnClientInstanceManager->isOwnSettingsDirty())
                return;

            qnSettings->load();
            qnClientInstanceManager->markOwnSettingsDirty(false);
        });

    timer->start();

    connect(qnSettings, &QnClientSettings::saved, this,
        [this]
        {
            qnClientInstanceManager->markOtherSettingsDirty(true);
        });
}
