#include "skin.h"

#include <QtGui/QImage>
#include <QtGui/QMessageBox>
#include <QtGui/QPixmapCache>

#ifdef CL_DEFAULT_SKIN_PREFIX
#  define CL_SKIN_PREFIX QLatin1String(CL_DEFAULT_SKIN_PREFIX)
#else
#  define CL_SKIN_PREFIX QLatin1Char(':')
#endif

static inline QPixmap cachedPixmap(const QString &name_, const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode)
{
    static bool initialized = false;
    if (!initialized) {
        QPixmapCache::setCacheLimit(16 * 1024); // 16 MB
        initialized = true;
    }

    QString name = name_;
    if (!size.isEmpty())
        name += QString(QLatin1String("_%1x%2_%3_%4")).arg(int(size.width())).arg(int(size.height())).arg(int(aspectMode)).arg(int(mode));

    QPixmap pm;
    if (!QPixmapCache::find(name, &pm)) {
        pm = QPixmap::fromImage(QImage(name_), Qt::OrderedDither | Qt::OrderedAlphaDither);
        if (!pm.isNull()) {
            if (!size.isEmpty())
                pm = pm.scaled(size, aspectMode, mode);
        } else {
            qWarning("Skin: Cannot load image `%1`", qPrintable(name_));
            QMessageBox::warning(0, QMessageBox::tr("Error"), QMessageBox::tr("Cannot load image `%1`").arg(name_), QMessageBox::Ok);
        }

        QPixmapCache::insert(name, pm);
    }

    return pm;
}

QString Skin::path(const QString &name)
{
    return CL_SKIN_PREFIX + QLatin1String("/skin/") + name;
}

QIcon Skin::icon(const QString &name)
{
    return QIcon(path(name));
}

QPixmap Skin::pixmap(const QString &name, const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode)
{
    return cachedPixmap(path(name), size, aspectMode, mode);
}
