#include "skin.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QImage>
#include <QtGui/QPixmapCache>
#include <QtWidgets/QStyleFactory>

#include <QtCore/QPluginLoader>
#include <QtWidgets/QApplication>

#include <iostream>

#include <utils/common/warnings.h>

#include "noptix_style.h"
#include "noptix_icon_loader.h"
#include "nx_style.h"

namespace {

static const QSize kHugeSize(100000, 100000);

} // namespace

QnSkin::QnSkin(QObject* parent): QObject(parent)
{
    init(QStringList());
}

QnSkin::QnSkin(const QStringList& paths, QObject* parent): QObject(parent)
{
    init(paths);
}

void QnSkin::init(const QStringList& paths)
{
    if (paths.isEmpty())
    {
        init(QStringList() << lit(":/skin"));
        return;
    }

    m_iconLoader = new QnNoptixIconLoader(this);

    m_paths = paths;
    for (int i = 0; i < m_paths.size(); i++)
    {
        m_paths[i] = QDir::toNativeSeparators(m_paths[i]);

        if (!m_paths[i].endsWith(QDir::separator()))
            m_paths[i] += QDir::separator();
    }

    int cacheLimit = 64 * 1024; // 64 MB
    if (QPixmapCache::cacheLimit() < cacheLimit)
        QPixmapCache::setCacheLimit(cacheLimit);
}

QnSkin::~QnSkin()
{
}

const QStringList& QnSkin::paths() const
{
    return m_paths;
}

QString QnSkin::path(const QString& name) const
{
    for (int i = m_paths.size() - 1; i >= 0; i--)
    {
        QString path = m_paths[i] + name;
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

QString QnSkin::path(const char* name) const
{
    return path(QLatin1String(name));
}

bool QnSkin::hasFile(const QString& name) const
{
    return !path(name).isEmpty();
}

bool QnSkin::hasFile(const char* name) const
{
    return hasFile(QLatin1String(name));
}

QIcon QnSkin::icon(const QString& name,
    const QString& checkedName,
    const QnIcon::SuffixesList* suffixes)
{
    return m_iconLoader->load(name, checkedName, suffixes);
}

QIcon QnSkin::icon(const char* name, const char* checkedName)
{
    return icon(QLatin1String(name), QLatin1String(checkedName));
}

QIcon QnSkin::icon(const QIcon& icon)
{
    return m_iconLoader->polish(icon);
}


QPixmap QnSkin::pixmap(const char* name,
    const QSize& size,
    Qt::AspectRatioMode aspectMode,
    Qt::TransformationMode mode)
{
    return pixmap(QString::fromLatin1(name), size, aspectMode, mode);
}

QPixmap QnSkin::pixmap(const QString& name,
    const QSize& size,
    Qt::AspectRatioMode aspectMode,
    Qt::TransformationMode mode)
{
    static const auto kHiDpiSuffix = lit("@2x");

    if (isHiDpi())
    {
        QFileInfo info(name);
        const auto suffix = info.completeSuffix();
        const auto newName = info.path() + lit("/") + info.completeBaseName() + kHiDpiSuffix
            + (suffix.isEmpty() ? QString() : lit(".") + info.suffix());
        auto result = getPixmapInternal(newName, size, aspectMode, mode);
        if (!result.isNull())
            return result;
    }

    return getPixmapInternal(name, size, aspectMode, mode);
}

QPixmap QnSkin::getPixmapInternal(const QString& name, const QSize& size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode)
{
    QString key = name;
    if (!size.isEmpty())
        key += QString(QLatin1String("_%1x%2_%3_%4")).arg(int(size.width())).arg(int(size.height())).arg(int(aspectMode)).arg(int(mode));

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap))
    {
        pixmap = QPixmap::fromImage(QImage(path(name)), Qt::OrderedDither | Qt::OrderedAlphaDither);
        pixmap.setDevicePixelRatio(1); // Force to use not scaled images
        if (!pixmap.isNull())
        {
            if (!size.isEmpty() && size != pixmap.size())
                pixmap = pixmap.scaled(size, aspectMode, mode);
        }
        else if (!name.contains(lit("@2x")))
        {
            qnWarning("Cannot load image '%1'", name);
        }

        QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
}

QStyle* QnSkin::newStyle(const QnGenericPalette& genericPalette)
{
    QnNxStyle* style = new QnNxStyle();
    style->setGenericPalette(genericPalette);
    return new QnNoptixStyle(style);
}

QMovie* QnSkin::newMovie(const QString& name, QObject* parent)
{
    return new QMovie(path(name), QByteArray(), parent);
}

QMovie* QnSkin::newMovie(const char* name, QObject* parent)
{
    return newMovie(QLatin1String(name), parent);
}

QSize QnSkin::maximumSize(const QIcon& icon, QIcon::Mode mode, QIcon::State state)
{
    int scale = isHiDpi() ? 2 : 1; //< we have only 1x and 2x scale icons
    return icon.actualSize(kHugeSize, mode, state) / scale;
}

QPixmap QnSkin::maximumSizePixmap(const QIcon& icon, QIcon::Mode mode, QIcon::State state)
{
    return icon.pixmap(kHugeSize, mode, state);
}

bool QnSkin::isHiDpi()
{
    return qApp->devicePixelRatio() > 1.0;
}
