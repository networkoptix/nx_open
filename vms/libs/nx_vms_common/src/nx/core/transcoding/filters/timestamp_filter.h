// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qnamespace.h>
#include <transcoding/filters/abstract_image_filter.h>

namespace nx {
namespace core {
namespace transcoding {

struct TimestampOverlaySettings;

class NX_VMS_COMMON_API TimestampFilter: public QnAbstractImageFilter
{
public:
    TimestampFilter(const core::transcoding::TimestampOverlaySettings& params);
    virtual ~TimestampFilter();

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& sourceSize) override;

    static QString timestampTextUtc(
        qint64 sinceEpochMs,
        int displayOffsetMs,
        Qt::DateFormat format);

    static QString timestampTextSimple(qint64 timeOffsetMs);

private:
    class Internal;
    const QScopedPointer<Internal> d;
};

} // namespace transcoding
} // namespace core
} // namespace nx
