// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>

#include <utils/common/aspect_ratio.h>

namespace nx::vms::client::core {

/**
 * Fit specified highlight rectangle into target aspect ratio window, return crop rectangle and
 * new highlight rectangle (within the cropped area).
 */
NX_VMS_CLIENT_CORE_API void calculatePreviewRects(
    const QnAspectRatio& resourceAr,
    const QRectF& highlightRect,
    const QnAspectRatio& targetAr,
    QRectF& cropRect,
    QRectF& croppedHighlightRect);

} // namespace nx::vms::client::core
