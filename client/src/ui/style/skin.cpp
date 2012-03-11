#include "skin.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QImage>
#include <QtGui/QPixmapCache>
#include <QtGui/QStyleFactory>

#include <utils/common/warnings.h>

#include "noptix_style.h"

#ifndef QN_SKIN_PATH
#  ifdef CL_CUSTOMIZATION_PRESET
#    define QN_SKIN_PATH QLatin1String(":/") + QLatin1String(CL_CUSTOMIZATION_PRESET)
#  else
#    define QN_SKIN_PATH QLatin1Char(':')
#  endif
#endif

namespace {
    void addPixmap(QIcon *icon, QIcon::Mode mode, const QString &path) {
        QPixmap pixmap = qnSkin->pixmap(path);
        icon->addPixmap(pixmap, mode, QIcon::On);
        icon->addPixmap(pixmap, mode, QIcon::Off);
    }

    void addPixmap(QIcon *icon, QIcon::State state, const QString &path) {
        QPixmap pixmap = qnSkin->pixmap(path);
        icon->addPixmap(pixmap, QIcon::Normal,   state);
        icon->addPixmap(pixmap, QIcon::Disabled, state);
        icon->addPixmap(pixmap, QIcon::Active,   state);
        icon->addPixmap(pixmap, QIcon::Selected, state);
    }

    void addPixmap(QIcon *icon, QIcon::Mode mode, QIcon::State state, const QString &path) {
        QPixmap pixmap = qnSkin->pixmap(path);
        icon->addPixmap(pixmap, mode, state);
    }

    void decompose(const QString &path, QString *prefix, QString *suffix) {
        QFileInfo info(path);
        *prefix = info.path() + QChar('/') + info.baseName();
        *suffix = info.completeSuffix();
        if(!suffix->isEmpty())
            *suffix = QChar('.') + *suffix;
    }

} // anonymous namespace

Q_GLOBAL_STATIC(QnSkin, qn_skinInstance);

QnSkin::QnSkin() {
    QPixmapCache::setCacheLimit(16 * 1024); // 16 MB
}

QnSkin *QnSkin::instance() {
    return qn_skinInstance();
}

QString QnSkin::path(const QString &name)
{
    if (name.isEmpty())
        return name;
    return QN_SKIN_PATH + QLatin1String("/skin/") + name;
}

QIcon QnSkin::icon(const QString &name, const QString &checkedName)
{
    QString prefix, suffix, path;
    QIcon icon(pixmap(name));

    decompose(name, &prefix, &suffix);

    path = prefix + QLatin1String("_hovered") + suffix;
    if(hasPixmap(path)) 
        addPixmap(&icon, QIcon::Active, path);
    
    path = prefix + QLatin1String("_disabled") + suffix;
    if(hasPixmap(path))
        addPixmap(&icon, QIcon::Disabled, path);
    
    path = prefix + QLatin1String("_selected") + suffix;
    if(hasPixmap(path))
        addPixmap(&icon, QIcon::Selected, path);

    decompose(checkedName.isEmpty() ? prefix + QLatin1String("_checked") + suffix : checkedName, &prefix, &suffix);

    path = prefix + suffix;
    if(hasPixmap(path))
        addPixmap(&icon, QIcon::On, path);
    
    path = prefix + QLatin1String("_hovered") + suffix;
    if(hasPixmap(path))
        addPixmap(&icon, QIcon::Active, QIcon::On, path);

    path = prefix + QLatin1String("_disabled") + suffix;
    if(hasPixmap(path))
        addPixmap(&icon, QIcon::Disabled, QIcon::On, path);

    path = prefix + QLatin1String("_selected") + suffix;
    if(hasPixmap(path))
        addPixmap(&icon, QIcon::Selected, QIcon::On, path);

    return icon;
}

bool QnSkin::hasPixmap(const QString &name) 
{
    return QFile::exists(path(name));
}

QPixmap QnSkin::pixmap(const QString &name, const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode)
{
    QString key = name;
    if (!size.isEmpty())
        key += QString(QLatin1String("_%1x%2_%3_%4")).arg(int(size.width())).arg(int(size.height())).arg(int(aspectMode)).arg(int(mode));

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
        pixmap = QPixmap::fromImage(QImage(path(name)), Qt::OrderedDither | Qt::OrderedAlphaDither);
        if (!pixmap.isNull()) {
            if (!size.isEmpty() && size != pixmap.size())
                pixmap = pixmap.scaled(size, aspectMode, mode);
        } else {
            qnWarning("Cannot load image '%1'", name);
        }

        QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
}

QStyle *QnSkin::style()
{
    QString baseStyleName;
#ifndef Q_OS_DARWIN
#   ifdef CL_CUSTOMIZATION_PRESET
    qputenv("BESPIN_PRESET", CL_CUSTOMIZATION_PRESET);
#   endif
    baseStyleName = QLatin1String("Bespin");
#else
    baseStyleName = QLatin1String("cleanlooks");
#endif

    QStyle *baseStyle = QStyleFactory::create(baseStyleName);
    QnNoptixStyle *style = new QnNoptixStyle(baseStyle);

    return style;
}
