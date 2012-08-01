#ifndef QN_SKIN_H
#define QN_SKIN_H

#include <QtCore/QHash>

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

class QStyle;

class QnSkin {
public:
    static const QIcon::Mode Normal = QIcon::Normal;
    static const QIcon::Mode Disabled = QIcon::Disabled;
    static const QIcon::Mode Active = QIcon::Active;
    static const QIcon::Mode Selected = QIcon::Selected;
    static const QIcon::Mode Pressed = static_cast<QIcon::Mode>(0xF);

    static const QIcon::State On = QIcon::On;
    static const QIcon::State Off = QIcon::Off;

    QnSkin();

    QIcon icon(const QString &name, const QString &checkedName);
    
    QIcon icon(const char *name, const char *checkedName = NULL) { 
        return icon(QLatin1String(name), QLatin1String(checkedName)); 
    }

    QPixmap pixmap(const QString &name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);
    
    QPixmap pixmap(const char *name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation) { 
        return pixmap(QLatin1String(name), size, aspectMode, mode); 
    }

    bool hasPixmap(const QString &name) const;

    bool hasPixmap(const char *name) const {
        return hasPixmap(QLatin1String(name));
    }

    QPixmap pixmap(const QIcon &icon, const QSize &size, QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    QPixmap pixmap(const QIcon &icon, int w, int h, QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    QPixmap pixmap(const QIcon &icon, int extent, QIcon::Mode mode = Normal, QIcon::State state = Off) const;

    QStyle *style();

    static QnSkin *instance();

private:
    QString path(const QString &name) const;

private:
    QHash<QString, QIcon> m_iconByNames;
    QHash<qint64, QIcon> m_pressedIconByKey;
};

#define qnSkin (QnSkin::instance())

#endif // QN_SKIN_H
