#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <client_core/connection_context_aware.h>
#include <api/server_rest_connection_fwd.h>

#include "abstract_thumbnail_provider.h"

namespace nx::vms::client::core {

class LiveThumbnailProvider:
    public QObject,
    public AbstractThumbnailProvider,
    public QnConnectionContextAware
{
    Q_OBJECT
    Q_PROPERTY(int thumbnailsHeight READ thumbnailsHeight WRITE setThumbnailsHeight
        NOTIFY thumbnailsHeightChanged)
    Q_PROPERTY(int rotation READ rotation WRITE setRotation NOTIFY rotationChanged)

public:
    LiveThumbnailProvider(QObject* parent = nullptr);
    virtual ~LiveThumbnailProvider() override;

    virtual QPixmap pixmap(const QString& thumbnailId) const override;

    Q_INVOKABLE void refresh(const QnUuid& cameraId);
    Q_INVOKABLE void refresh(const QString& cameraId);

    int thumbnailsHeight() const { return m_thumbnailHeight; }
    void setThumbnailsHeight(int height);

    int rotation() const { return m_rotation; }
    void setRotation(int rotation);

signals:
    void thumbnailsHeightChanged();
    void rotationChanged();
    void thumbnailUpdated(
        const QnUuid& cameraId, const QString& thumbnailId, const QUrl& thumbnailUrl);

public:
    static void registerQmlType();

private:
    struct ThumbnailData
    {
        QString id;
        rest::Handle requestId = -1;
    };

    mutable nx::utils::Mutex m_mutex;
    QHash<QnUuid, ThumbnailData> m_dataByCameraId;
    QHash<QString, QPixmap> m_pixmapById;
    QThread* m_decompressionThread = nullptr;
    int m_thumbnailHeight = 240;
    int m_rotation = -1;
};

} // namespace nx::vms::client::core
