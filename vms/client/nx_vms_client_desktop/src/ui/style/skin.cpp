#include "skin.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>
#include <QtWidgets/QStyleFactory>

#include <QtCore/QPluginLoader>
#include <QtWidgets/QApplication>

#include <QtSvg/QSvgRenderer>

#include <iostream>

#include "noptix_style.h"
#include "noptix_icon_loader.h"
#include "nx_style.h"

#include <nx/utils/log/assert.h>

namespace {

static const QSize kHugeSize(100000, 100000);

void correctPixelRatio(QPixmap& pixmap)
{
    if (QnSkin::isHiDpi())
        pixmap.setDevicePixelRatio(2.0);
}

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

    const qreal dpr = qApp->devicePixelRatio();
    const int cacheLimit = qRound(64 * 1024 // 64 MB
        * dpr * dpr); // dpi-dcaled
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
    if (QDir::isAbsolutePath(name))
        return QFile::exists(name) ? name : QString();

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

QPixmap QnSkin::pixmap(const QString& name, bool correctDevicePixelRatio, const QSize& desiredSize)
{
    if (name.endsWith(".svg"))
    {
        if (desiredSize.isEmpty())
            return maximumSizePixmap(icon(name), QIcon::Normal, QIcon::Off, correctDevicePixelRatio);

        return getPixmapFromSvgInternal(name, correctDevicePixelRatio, desiredSize);
    }

    static const auto kHiDpiSuffix = lit("@2x");

    auto result =
        [this, name]()
        {
            if (isHiDpi())
            {
                QFileInfo info(name);
                const auto suffix = info.completeSuffix();
                const auto newName = info.path() + L'/' + info.completeBaseName() + kHiDpiSuffix
                    + (suffix.isEmpty() ? QString() : L'.' + info.suffix());
                auto result = getPixmapInternal(newName);
                if (!result.isNull())
                    return result;
            }

            return getPixmapInternal(name);
        }();

    if (correctDevicePixelRatio)
        correctPixelRatio(result);

    return result;
}

QPixmap QnSkin::getPixmapInternal(const QString& name)
{
    QPixmap pixmap;
    if (!QPixmapCache::find(name, &pixmap))
    {
        const auto fullPath = path(name);
        if (fullPath.isEmpty())
        {
            NX_ASSERT(name.contains("@2x"));
        }
        else
        {
            pixmap = QPixmap::fromImage(QImage(fullPath), Qt::OrderedDither | Qt::OrderedAlphaDither);
            pixmap.setDevicePixelRatio(1); // Force to use not scaled images
            NX_ASSERT(!pixmap.isNull());
        }
        QPixmapCache::insert(name, pixmap);
    }
    return pixmap;
}

QPixmap QnSkin::getPixmapFromSvgInternal(const QString& name, bool correctDevicePixelRatio, const QSize& size)
{
    const auto key = name + QString(":%1:%2:%3").arg(correctDevicePixelRatio).arg(size.width()).arg(size.height());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap))
    {
        QFile source(path(name));
        if (!source.open(QIODevice::ReadOnly))
        {
            NX_ASSERT(false, "Cannot load svg");
            return pixmap; //< Cache it?
        }

        const QByteArray data = source.readAll();

        QSvgRenderer renderer;
        if (!renderer.load(data))
        {
            NX_ASSERT(false, "Error while loading svg");
            return pixmap; //< Cache it?;
        }

        const QSize& correctedSize = correctDevicePixelRatio
            ? size * qApp->devicePixelRatio()
            : size;

        pixmap = QPixmap(correctedSize);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        renderer.render(&p);

        if (correctDevicePixelRatio)
            pixmap.setDevicePixelRatio(qApp->devicePixelRatio());
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

QPixmap QnSkin::maximumSizePixmap(const QIcon& icon, QIcon::Mode mode,
    QIcon::State state, bool correctDevicePixelRatio)
{
    auto pixmap = icon.pixmap(kHugeSize, mode, state);
    if (correctDevicePixelRatio)
        correctPixelRatio(pixmap);
    return pixmap;
}

bool QnSkin::isHiDpi()
{
    return qApp->devicePixelRatio() > 1.0;
}

QPixmap QnSkin::colorize(const QPixmap& source, const QColor& color)
{
    QPixmap result(source);
    result.detach();

    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(QRect(QPoint(), result.size() / result.devicePixelRatio()), color);

    return result;
}
