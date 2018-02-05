#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <nx/api/mediaserver/image_request.h>

#include "image_provider.h"

namespace nx {
namespace client {
namespace desktop {

class CameraThumbnailProvider: public QnImageProvider, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QnImageProvider;

public:
    explicit CameraThumbnailProvider(const api::CameraImageRequest& request,
        QObject* parent = nullptr);

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    api::CameraImageRequest requestData() const;
    void setRequestData(const api::CameraImageRequest& data);

signals:
    /** Internal signal to implement thread-safety. */
    void imageDataLoadedInternal(const QByteArray &data);

protected:
    virtual void doLoadAsync() override;

private:
    void setStatus(Qn::ThumbnailStatus status);

private:
    api::CameraImageRequest m_request;
    QImage m_image;
    Qn::ThumbnailStatus m_status;
};

} // namespace desktop
} // namespace client
} // namespace nx
