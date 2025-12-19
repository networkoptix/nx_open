// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_loader_delegate.h"

#include <nx/utils/log/format.h>

namespace nx::vms::client::mobile {
namespace timeline {

QString AbstractLoaderDelegate::makeImageRequest(
    const nx::Uuid& cameraId, qint64 timestampMs, int resolution)
{
    const QString requestTemplate = "ec2/cameraThumbnails?cameraId=%1&time=%2"
        "&width=%3&height=0&method=precise&imageFormat=jpg";

    return nx::format(requestTemplate, cameraId.toSimpleString(), timestampMs, resolution);
}

} // namespace timeline
} // namespace nx::vms::client::mobile
