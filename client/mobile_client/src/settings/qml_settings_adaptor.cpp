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

} // namespace mobile
} // namespace client
} // namespace nx
