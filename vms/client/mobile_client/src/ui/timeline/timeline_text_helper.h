// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QFont>
#include <QtGui/QImage>

class QnTimelineTextHelperPrivate;

class QnTimelineTextHelper
{
public:
    QnTimelineTextHelper(const QFont& font,
                         const QColor& color,
                         const QStringList& strings);
    ~QnTimelineTextHelper();

    QRectF symbolCoordinates(QChar symbol) const;
    QRectF stringCoordinates(const QString& string) const;
    QSize stringSize(const QString& string) const;
    QImage texture() const;
    int maxCharWidth() const;
    int lineHeight() const;
    int spaceWidth() const;

private:
    QScopedPointer<QnTimelineTextHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnTimelineTextHelper)
};
