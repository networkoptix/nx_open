#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_globals.h>

#include <nx/api/mediaserver/image_request.h>

#include "image_provider.h"

namespace nx::vms::client::desktop {

/**
* This class allows receiving of thumbnails via http request to server or from local files.
* Every setRequest() call will bring a new screenshot.
*/
class ResourceThumbnailProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    explicit ResourceThumbnailProvider(const nx::api::ResourceImageRequest& request,
        QObject* parent = nullptr);
    virtual ~ResourceThumbnailProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    nx::api::ResourceImageRequest requestData() const;
    void setRequestData(const nx::api::ResourceImageRequest& data);

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
