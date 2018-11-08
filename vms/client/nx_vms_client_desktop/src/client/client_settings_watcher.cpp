#include "client_settings_watcher.h"

#include <QtCore/QTimer>

#include <client/client_settings.h>
#include <client/client_instance_manager.h>
#include <client/client_show_once_settings.h>

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
            qnClientShowOnce->sync();
            qnClientInstanceManager->markOwnSettingsDirty(false);
        });

    timer->start();

    auto invalidateOther = [this] { qnClientInstanceManager->markOtherSettingsDirty(true); };

    connect(qnSettings, &QnClientSettings::saved, this, invalidateOther);
    connect(qnClientShowOnce, &nx::settings::ShowOnce::changed, this, invalidateOther);
}
