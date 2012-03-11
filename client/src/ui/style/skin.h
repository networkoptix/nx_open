#ifndef SKIN_H
#define SKIN_H

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

class QStyle;

class Skin
{
public:
    static QIcon icon(const QString &name);
    
    static QIcon icon(const char *name) { 
        return icon(QLatin1String(name)); 
    }

    static QPixmap pixmap(const QString &name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);
    
    static QPixmap pixmap(const char *name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation) { 
        return pixmap(QLatin1String(name), size, aspectMode, mode); 
    }

    static bool hasPixmap(const QString &name);

    static bool hasPixmap(const char *name) {
        return hasPixmap(QLatin1String(name));
    }

    static QStyle *style();

private:
    static QString path(const QString &name);
};

#endif // SKIN_H
