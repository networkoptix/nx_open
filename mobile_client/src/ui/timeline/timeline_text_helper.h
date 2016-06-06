#pragma once

#include <QtGui/QImage>
#include <QtGui/QFont>

class QnTimelineTextHelperPrivate;

class QnTimelineTextHelper
{
public:
    QnTimelineTextHelper(const QFont& font,
                         const QColor& color,
                         const QStringList& strings);
    ~QnTimelineTextHelper();

    QRectF digitCoordinates(int digit) const;
    QRectF stringCoordinates(const QString& string) const;
    QSize stringSize(const QString& string) const;
    QSize digitSize() const;
    QImage texture() const;
    int maxCharWidth() const;
    int lineHeight() const;
    int spaceWidth() const;

private:
    QScopedPointer<QnTimelineTextHelperPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnTimelineTextHelper)
};
