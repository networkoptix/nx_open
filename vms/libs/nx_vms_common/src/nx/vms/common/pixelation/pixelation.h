// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <nx/utils/impl_ptr.h>

class QThread;

namespace nx::vms::common::pixelation {

class NX_VMS_COMMON_API Pixelation
{
public:
    Pixelation();
    ~Pixelation();

    QImage pixelate(const QImage& source, const QVector<QRectF>& rectangles, double intensity);
    QThread* thread() const;

private:
    struct Private;
    utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common::pixelation
