// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_image_filter.h"
#include "utils/media/frame_info.h"

NX_VMS_COMMON_API CLVideoDecoderOutputPtr rotateImage(
    const CLConstVideoDecoderOutputPtr& frame, int angle);

class NX_VMS_COMMON_API QnRotateImageFilter: public QnAbstractImageFilter
{
public:
    QnRotateImageFilter(int angle);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

    int angle() const { return m_angle; }
private:
    int m_angle;
};
