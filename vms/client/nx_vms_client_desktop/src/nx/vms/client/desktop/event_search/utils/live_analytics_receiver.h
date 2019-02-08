#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <nx/streaming/media_data_packet.h>

namespace nx::vms::client::desktop {

class LiveAnalyticsReceiver:
    public QObject,
    public QnConnectionContextAware
{
    Q_OBJECT

public:
    LiveAnalyticsReceiver(QObject* parent = nullptr);
    LiveAnalyticsReceiver(const QnVirtualCameraResourcePtr& camera, QObject* parent = nullptr);
    virtual ~LiveAnalyticsReceiver() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& value);

    QList<QnAbstractCompressedMetadataPtr> takeData();

signals:
    void dataOverflow(QPrivateSignal); //< takeData must be called as soon as possible.

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
