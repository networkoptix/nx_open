// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "preview_rect_calculator.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

void calculatePreviewRects(
    const QnAspectRatio& resourceAr,
    const QRectF& highlightRect,
    const QnAspectRatio& targetAr,
    QRectF& cropRect,
    QRectF& croppedHighlightRect)
{
    if (!NX_ASSERT(resourceAr.isValid() && targetAr.isValid()) || !highlightRect.isValid())
    {
        cropRect = {};
        croppedHighlightRect = {};
        return;
    }

    const qreal sourceAr = resourceAr.toFloat();
    const qreal previewAr = targetAr.toFloat();
    const qreal highlightAr = sourceAr * highlightRect.width() / highlightRect.height();

    if (previewAr > highlightAr)
    {
        const qreal width = std::min(highlightRect.height() * previewAr / sourceAr, 1.0);
        const qreal x = std::clamp(highlightRect.left() + (highlightRect.width() - width) / 2,
            0.0, 1.0 - width);

        cropRect = QRectF(x, highlightRect.top(), width, highlightRect.height());
    }
    else
    {
        const qreal height = std::min(highlightRect.width() / previewAr * sourceAr, 1.0);
        const qreal y = std::clamp(highlightRect.top() + (highlightRect.height() - height) / 2,
            0.0, 1.0 - height);

        cropRect = QRectF(highlightRect.left(), y, highlightRect.width(), height);
    }

    croppedHighlightRect = QRectF(
        (highlightRect.left() - cropRect.left()) / cropRect.width(),
        (highlightRect.top() - cropRect.top()) / cropRect.height(),
        highlightRect.width() / cropRect.width(),
        highlightRect.height() / cropRect.height());
}

} // namespace nx::vms::client::core
