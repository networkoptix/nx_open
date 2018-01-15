#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_globals.h>

#include <nx/api/mediaserver/image_request.h>

#include "image_provider.h"

namespace nx {
namespace client {
namespace desktop {

class ResourceThumbnailProvider: public QnImageProvider
{
    Q_OBJECT
    using base_type = QnImageProvider;

public:
    explicit ResourceThumbnailProvider(const api::ResourceImageRequest& request,
        QObject* parent = nullptr);
    virtual ~ResourceThumbnailProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    api::ResourceImageRequest requestData() const;
    void setRequestData(const api::ResourceImageRequest& data);

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
