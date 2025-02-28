// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtGui/QImage>

namespace nx::utils {

class AbstractTextRenderer
{
public:
    virtual ~AbstractTextRenderer() = default;

    virtual void drawText(QImage& image, const QRect& rect, const QString& text) = 0;
    virtual QSize textSize(const QString& text) = 0;

public:
    QColor color;
    int fontPixelSize = 12;
    QString fontFamily;
};

class TextRendererFactory
{
public:
    static std::unique_ptr<AbstractTextRenderer> create();
};

} // nx::utils
