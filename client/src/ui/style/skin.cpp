#include "skin.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QImage>
#include <QtGui/QPixmapCache>
#include <QtGui/QStyleFactory>

#include <utils/common/warnings.h>

#include "noptix_style.h"

namespace {
    class QnIconBuilder {
        typedef QHash<QPair<QIcon::Mode, QIcon::State>, QPixmap> container_type;

    public:
        void addPixmap(const QPixmap &pixmap, QIcon::Mode mode) {
            addPixmap(pixmap, mode, QIcon::On);
            addPixmap(pixmap, mode, QIcon::Off);
        }

        void addPixmap(const QPixmap &pixmap, QIcon::State state) {
            addPixmap(pixmap, QIcon::Normal,   state);
            addPixmap(pixmap, QIcon::Disabled, state);
            addPixmap(pixmap, QIcon::Active,   state);
            addPixmap(pixmap, QIcon::Selected, state);
        }

        void addPixmap(const QPixmap &pixmap, QIcon::Mode mode, QIcon::State state) {
            m_pixmaps[qMakePair(mode, state)] = pixmap;
        }

        QIcon createIcon() const {
            QIcon icon(m_pixmaps.value(qMakePair(QIcon::Normal, QIcon::Off)));

            for(container_type::const_iterator pos = m_pixmaps.begin(), end = m_pixmaps.end(); pos != end; pos++)
                icon.addPixmap(pos.value(), pos.key().first, pos.key().second);

            return icon;
        }

    private:
        container_type m_pixmaps;
    };

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
    QPixmapCache::setCacheLimit(64 * 1024); // 64 MB
}

QnSkin *QnSkin::instance() {
    return qn_skinInstance();
}

QIcon QnSkin::icon(const QString &name, const QString &checkedName) {
    QString key = name + QLatin1String("=^_^=") + checkedName;
    if(m_iconByNames.contains(key))
        return m_iconByNames.value(key);

    QString prefix, suffix, path;

    decompose(name, &prefix, &suffix);
    QString pressedPath = prefix + QLatin1String("_pressed") + suffix;

    /* Create normal icon. */
    QnIconBuilder builder;
    builder.addPixmap(pixmap(name), Normal, Off);

    path = prefix + QLatin1String("_hovered") + suffix;
    if(hasPixmap(path)) 
        builder.addPixmap(pixmap(path), Active);
    
    path = prefix + QLatin1String("_disabled") + suffix;
    if(hasPixmap(path))
        builder.addPixmap(pixmap(path), Disabled);
    
    path = prefix + QLatin1String("_selected") + suffix;
    if(hasPixmap(path))
        builder.addPixmap(pixmap(path), Selected);

    decompose(checkedName.isEmpty() ? prefix + QLatin1String("_checked") + suffix : checkedName, &prefix, &suffix);
    QString checkedPressedPath = prefix + QLatin1String("_pressed") + suffix;

    path = prefix + suffix;
    if(hasPixmap(path))
        builder.addPixmap(pixmap(path), On);
    
    path = prefix + QLatin1String("_hovered") + suffix;
    if(hasPixmap(path))
        builder.addPixmap(pixmap(path), Active, On);

    path = prefix + QLatin1String("_disabled") + suffix;
    if(hasPixmap(path))
        builder.addPixmap(pixmap(path), Disabled, On);

    path = prefix + QLatin1String("_selected") + suffix;
    if(hasPixmap(path))
        builder.addPixmap(pixmap(path), Selected, On);

    QIcon icon = builder.createIcon();

    /* Create pressed icon. */
    QnIconBuilder pressedBuilder;
    pressedBuilder.addPixmap(icon.pixmap(QSize(1024, 1024), Normal, Off), Off);
    pressedBuilder.addPixmap(icon.pixmap(QSize(1024, 1024), Normal, On), On);

    if(hasPixmap(pressedPath))
        pressedBuilder.addPixmap(pixmap(pressedPath), Normal);
        
    if(hasPixmap(checkedPressedPath)) 
        pressedBuilder.addPixmap(pixmap(checkedPressedPath), Normal, On);

    QIcon pressedIcon = pressedBuilder.createIcon();

    /* Save & return. */
    m_iconByNames[key] = icon;
    m_pressedIconByKey[icon.cacheKey()] = pressedIcon;
    return icon;
}

bool QnSkin::hasPixmap(const QString &name) const {
    return QFile::exists(path(name));
}

QPixmap QnSkin::pixmap(const QString &name, const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode) {
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

QPixmap	QnSkin::pixmap(const QIcon &icon, const QSize &size, QIcon::Mode mode, QIcon::State state) const {
    if(mode == Pressed) {
        return m_pressedIconByKey.value(icon.cacheKey(), icon).pixmap(size, Normal, state);
    } else {
        return icon.pixmap(size, mode, state);
    }
}

QPixmap	QnSkin::pixmap(const QIcon &icon, int w, int h, QIcon::Mode mode, QIcon::State state) const {
    return pixmap(icon, QSize(w, h), mode, state);
}

QPixmap	QnSkin::pixmap(const QIcon &icon, int extent, QIcon::Mode mode, QIcon::State state) const {
    return pixmap(icon, QSize(extent, extent), mode, state);
}

QStyle *QnSkin::style() {
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

QString QnSkin::path(const QString &name) const {
    if (name.isEmpty())
        return name;
    return QN_SKIN_PATH + QLatin1String("/skin/") + name;
}
