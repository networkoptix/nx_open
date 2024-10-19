// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#if (QT_VERSION < QT_VERSION_CHECK(6, 9, 0))

    QT_BEGIN_NAMESPACE

    extern Q_WIDGETS_EXPORT void qt_blurImage(
        QImage& image,
        qreal radius,
        bool improvedQuality,
        int transposed = 0);

    QT_END_NAMESPACE

#endif
