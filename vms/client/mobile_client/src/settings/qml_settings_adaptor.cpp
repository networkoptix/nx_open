#include "qml_settings_adaptor.h"

#include <mobile_client/mobile_client_settings.h>

namespace nx {
namespace client {
namespace mobile {

QmlSettingsAdaptor::QmlSettingsAdaptor(QObject* parent):
    QObject(parent)
{
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
        [this](int id)
        {
            switch (id)
            {
                case QnMobileClientSettings::LiveVideoPreviews:
                    emit liveVideoPreviewsChanged();
                    break;

                case QnMobileClientSettings::LastUsedQuality:
                    emit lastUsedQualityChanged();
                    break;

                case QnMobileClientSettings::SavePasswords:
                    emit savePasswordsChanged();
                    break;

                default:
                    break;
            }
        });
}

bool QmlSettingsAdaptor::liveVideoPreviews() const
{
    return qnSettings->liveVideoPreviews();
}

void QmlSettingsAdaptor::setLiveVideoPreviews(bool enable)
{
    qnSettings->setLiveVideoPreviews(enable);
}

int QmlSettingsAdaptor::lastUsedQuality() const
{
    return qnSettings->lastUsedQuality();
}

void QmlSettingsAdaptor::setLastUsedQuality(int quality)
{
    qnSettings->setLastUsedQuality(quality);
}

bool QmlSettingsAdaptor::savePasswords() const
{
    return qnSettings->savePasswords();
}

void QmlSettingsAdaptor::setSavePasswords(bool value)
{
    qnSettings->setSavePasswords(value);
}

} // namespace mobile
} // namespace client
} // namespace nx
