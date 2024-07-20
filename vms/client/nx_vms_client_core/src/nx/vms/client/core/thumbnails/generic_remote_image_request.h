// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "async_image_result.h"

class QThreadPool;

namespace nx::vms::client::core {

class SystemContext;

/**
 * This class performs an HTTP request through the server connection from specified system context
 * and expects a binary image (image/png, image/jpg or image/tiff) or JSON error in response.
 */
class NX_VMS_CLIENT_CORE_API GenericRemoteImageRequest: public AsyncImageResult
{
    Q_OBJECT
    using base_type = AsyncImageResult;

public:
    explicit GenericRemoteImageRequest(
        SystemContext* systemContext,
        const QString& requestLine,
        QObject* parent = nullptr);

    virtual ~GenericRemoteImageRequest() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
