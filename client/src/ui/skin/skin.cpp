#include "skin.h"

#include <QtGui/QImage>
#include <QtGui/QPixmapCache>

#include "appstyle_p.h"

#ifndef CL_SKIN_PATH
#  ifdef CL_CUSTOMIZATION_PRESET
#    define CL_SKIN_PATH QLatin1String(":/") + QLatin1String(CL_CUSTOMIZATION_PRESET)
#  else
#    define CL_SKIN_PATH QLatin1Char(':')
#  endif
#endif

QString Skin::path(const QString &name)
{
    return CL_SKIN_PATH + QLatin1String("/skin/") + name;
}

QIcon Skin::icon(const QString &name)
{
    //return QIcon::fromTheme(name, QIcon(path(name)));
    return QIcon(path(name));
}

QPixmap Skin::pixmap(const QString &name, const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode)
{
    static bool initialized = false;
    if (!initialized) {
        QPixmapCache::setCacheLimit(16 * 1024); // 16 MB
        initialized = true;
    }

    QString key = name;
    if (!size.isEmpty())
        key += QString(QLatin1String("_%1x%2_%3_%4")).arg(int(size.width())).arg(int(size.height())).arg(int(aspectMode)).arg(int(mode));

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
        pixmap = QPixmap::fromImage(QImage(Skin::path(name)), Qt::OrderedDither | Qt::OrderedAlphaDither);
        if (!pixmap.isNull()) {
            if (!size.isEmpty() && size != pixmap.size())
                pixmap = pixmap.scaled(size, aspectMode, mode);
        } else {
            qWarning("Skin: Cannot load image `%s`", qPrintable(name));
        }

        QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
}

QStyle *Skin::style()
{
    QString styleName;
#ifndef Q_OS_DARWIN
#  ifdef CL_CUSTOMIZATION_PRESET
    qputenv("BESPIN_PRESET", CL_CUSTOMIZATION_PRESET);
#  endif
    styleName = QLatin1String("Bespin");
#else
    styleName = QLatin1String("cleanlooks");
#endif

    AppStyle *style = new AppStyle(styleName);
    (void)new AppProxyStyle(style->baseStyle());

    return style;
}
