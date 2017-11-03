#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtGui/QImage>

#include <api/server_rest_connection_fwd.h>

#include <client_core/connection_context_aware.h>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <utils/image_provider.h>

#include <nx/utils/uuid.h>

// TODO: #GDM create an application-wide set of thumbnails managers with different options: VMS-6759
class QnCameraThumbnailManager: public QnImageProvider, public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = QnImageProvider;
public:
    explicit QnCameraThumbnailManager(QObject* parent = nullptr);
    virtual ~QnCameraThumbnailManager();

    QnVirtualCameraResourcePtr selectedCamera() const;
    void selectCamera(const QnVirtualCameraResourcePtr& camera);
    void refreshSelectedCamera();

    bool autoRotate() const;
    void setAutoRotate(bool value);

    bool autoRefresh() const;
    void setAutoRefresh(bool value);

    QSize thumbnailSize() const;
    void setThumbnailSize(const QSize& size);

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    static QSize sizeHintForCamera(const QnVirtualCameraResourcePtr& camera, const QSize& limit);

    Qn::ThumbnailStatus statusForCamera(const QnVirtualCameraResourcePtr& camera) const;
    bool hasThumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const;
    QImage thumbnailForCamera(const QnVirtualCameraResourcePtr& camera) const;

signals:
    void thumbnailReady(const QnUuid& resourceId, const QPixmap& thumbnail);

protected:
    virtual void doLoadAsync() override;

private:
    void at_resPool_statusChanged(const QnResourcePtr& resource);
    void at_resPool_resourceRemoved(const QnResourcePtr& resource);

private:
    rest::Handle loadThumbnailForCamera(const QnVirtualCameraResourcePtr& camera);

    bool isUpdateRequired(const QnVirtualCameraResourcePtr& camera, Qn::ThumbnailStatus status) const;
    void forceRefreshThumbnails();

    QPixmap scaledPixmap(const QPixmap& pixmap) const;

    struct ThumbnailData
    {
        ThumbnailData();

        QImage thumbnail;
        Qn::ThumbnailStatus status;
        rest::Handle loadingHandle;
    };

    QHash<QnVirtualCameraResourcePtr, ThumbnailData> m_thumbnailByCamera;
    QnVirtualCameraResourcePtr m_selectedCamera;
    QSize m_thumbnailSize;
    QTimer* m_refreshingTimer;
    bool m_autoRotate = true;
};
