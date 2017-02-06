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

public:
    explicit QmlSettingsAdaptor(QObject* parent = nullptr);

    bool liveVideoPreviews() const;
    void setLiveVideoPreviews(bool enable);

signals:
    void liveVideoPreviewsChanged();
};

} // namespace mobile
} // namespace client
} // namespace nx
