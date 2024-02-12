// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtGui/QImage>

#include <api/server_rest_connection_fwd.h>
#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include "image_provider.h"

namespace nx::vms::client::desktop {

// TODO: #vkutin Use AbstractResourceThumbnail and its dependants instead of this class if needed.
class CameraThumbnailManager: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    explicit CameraThumbnailManager(QObject* parent = nullptr);
    virtual ~CameraThumbnailManager() override;

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
    void thumbnailReady(const nx::Uuid& resourceId, const QPixmap& thumbnail);

protected:
    virtual void doLoadAsync() override;

private:
    void atCameraStatusChanged(const QnResourcePtr& resource);
    void atResourcesRemoved(const QnResourceList& resources);

private:
    rest::Handle loadThumbnailForCamera(const QnVirtualCameraResourcePtr& camera);

    bool isUpdating(Qn::ThumbnailStatus status) const;

    bool isUpdateRequired(const QnVirtualCameraResourcePtr& camera,
        Qn::ThumbnailStatus status) const;

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
    QTimer* const m_refreshingTimer = nullptr;
    bool m_autoRotate = true;
};

} // namespace nx::vms::client::desktop
