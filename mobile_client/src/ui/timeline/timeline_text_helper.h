#ifndef QNTIMELINETEXTHELPER_H
#define QNTIMELINETEXTHELPER_H

#include <QtGui/QImage>
#include <QtGui/QFont>

class QnTimelineTextHelperPrivate;

class QnTimelineTextHelper
{
public:
    QnTimelineTextHelper(const QFont &font, qreal pixelRatio, const QColor &color, const QStringList &strings);
    ~QnTimelineTextHelper();

    QRectF digitCoordinates(int digit) const;
    QRectF stringCoordinates(const QString &string) const;
    QSize stringSize(const QString &string) const;
    QSize digitSize() const;
    QImage texture() const;
    int maxCharWidth() const;
    int lineHeight() const;
    int spaceWidth() const;
    qreal pixelRatio() const;

private:
    QScopedPointer<QnTimelineTextHelperPrivate> d;
};

#endif // QNTIMELINETEXTHELPER_H
