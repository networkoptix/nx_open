// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QAbstractTextDocumentLayout>

namespace nx::vms::client::core {

class ColorSquareTextObject: public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    enum
    {
        ColorSquareTextFormat = QTextFormat::UserObject + 1
    };

    enum Properties
    {
        Color = 1,
        Length
    };

    QSizeF intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format) override;

    void drawObject(
        QPainter* painter,
        const QRectF& rect,
        QTextDocument* doc,
        int posInDocument,
        const QTextFormat& format) override;

    void setColor(const QColor& value);
};

} // namespace nx::vms::client::core
