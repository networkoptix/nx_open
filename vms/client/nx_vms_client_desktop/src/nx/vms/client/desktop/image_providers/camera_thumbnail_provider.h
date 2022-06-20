// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/api/mediaserver/image_request.h>

#include "image_provider.h"

namespace nx::vms::client::desktop {

/**
* This class allows receiving of thumbnails via http request to server.
* Every setRequest() call will bring a new screenshot.
*/
class CameraThumbnailProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    explicit CameraThumbnailProvider(const nx::api::CameraImageRequest& request,
        QObject* parent = nullptr);

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    nx::api::CameraImageRequest requestData() const;
    void setRequestData(const nx::api::CameraImageRequest& data, bool resetStatus = false);

    qint64 timestampUs() const;

    virtual bool tryLoad() override;

    static const QString kFrameTimestampKey;
    static const QString kFrameFromPluginKey;

signals:
    /** Internal signal to implement thread-safety. */
    void imageDataLoadedInternal(const QByteArray &data, Qn::ThumbnailStatus nextStatus,
        qint64 timestampMs, bool frameFromPlugin);

protected:
    virtual void doLoadAsync() override;

private:
    void setStatus(Qn::ThumbnailStatus status);

private:
    nx::api::CameraImageRequest m_request;
    QImage m_image;
    Qn::ThumbnailStatus m_status;
    qint64 m_timestampUs = 0;
};

} // namespace nx::vms::client::desktop
