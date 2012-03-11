#ifndef QN_SKIN_H
#define QN_SKIN_H

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

class QStyle;

class QnSkin
{
public:
    QnSkin();

    QIcon icon(const QString &name, const QString &checkedName);
    
    QIcon icon(const char *name, const char *checkedName = NULL) { 
        return icon(QLatin1String(name), QLatin1String(checkedName)); 
    }

    QPixmap pixmap(const QString &name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);
    
    QPixmap pixmap(const char *name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation) { 
        return pixmap(QLatin1String(name), size, aspectMode, mode); 
    }

    bool hasPixmap(const QString &name);

    bool hasPixmap(const char *name) {
        return hasPixmap(QLatin1String(name));
    }

    QStyle *style();

    static QnSkin *instance();

private:
    QString path(const QString &name);
};

#define qnSkin (QnSkin::instance())

#endif // QN_SKIN_H
