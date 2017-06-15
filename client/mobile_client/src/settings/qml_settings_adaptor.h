#pragma once

#include <QtCore/QObject>

namespace nx {
namespace client {
namespace mobile {

class QmlSettingsAdaptor: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool liveVideoPreviews
        READ liveVideoPreviews WRITE setLiveVideoPreviews
        NOTIFY liveVideoPreviewsChanged)
    Q_PROPERTY(int lastUsedQuality
        READ lastUsedQuality WRITE setLastUsedQuality
        NOTIFY lastUsedQualityChanged)

public:
    explicit QmlSettingsAdaptor(QObject* parent = nullptr);

    bool liveVideoPreviews() const;
    void setLiveVideoPreviews(bool enable);

    int lastUsedQuality() const;
    void setLastUsedQuality(int quality);

signals:
    void liveVideoPreviewsChanged();
    void lastUsedQualityChanged();
};

} // namespace mobile
} // namespace client
} // namespace nx
