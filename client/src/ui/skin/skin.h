#ifndef SKIN_H
#define SKIN_H

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

class Skin
{
public:
    static QString path(const QString &name);
    static QIcon icon(const QString &name);
    static QPixmap pixmap(const QString &name, const QSize &size = QSize(),
                          Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio,
                          Qt::TransformationMode mode = Qt::FastTransformation);
};

#endif // SKIN_H
