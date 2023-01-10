// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "skin.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QPluginLoader>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <nx/utils/log/assert.h>

#include "icon_loader.h"
#include "old_style.h"
#include "style.h"

template<>
nx::vms::client::desktop::Skin* Singleton<nx::vms::client::desktop::Skin>::s_instance = nullptr;

namespace nx::vms::client::desktop {

namespace {

static const QSize kHugeSize(100000, 100000);
static const auto kHiDpiSuffix = "@2x";

void correctPixelRatio(QPixmap& pixmap)
{
    if (Skin::isHiDpi())
        pixmap.setDevicePixelRatio(2.0);
}

} // namespace

Skin::Skin(const QStringList& paths, QObject* parent): QObject(parent)
{
    init(paths);
}

void Skin::init(const QStringList& paths)
{
    if (paths.isEmpty())
    {
        init(QStringList() << ":/skin");
        return;
    }

    m_iconLoader = new IconLoader(this);

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

Skin::~Skin()
{
}

const QStringList& Skin::paths() const
{
    return m_paths;
}

QString Skin::path(const QString& name) const
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

QString Skin::path(const char* name) const
{
    return path(QLatin1String(name));
}

bool Skin::hasFile(const QString& name) const
{
    return !path(name).isEmpty();
}

bool Skin::hasFile(const char* name) const
{
    return hasFile(QLatin1String(name));
}

QIcon Skin::icon(const QString& name,
    const QString& checkedName,
    const QnIcon::Suffixes* suffixes,
    const SvgIconColorer::IconSubstitutions& svgColorSubstitutions)
{
    return m_iconLoader->load(name, checkedName, suffixes, svgColorSubstitutions);
}

QIcon Skin::icon(const char* name, const char* checkedName)
{
    return icon(QLatin1String(name), QLatin1String(checkedName));
}

QIcon Skin::icon(const QIcon& icon)
{
    return m_iconLoader->polish(icon);
}

QPixmap Skin::pixmap(const QString& name, bool correctDevicePixelRatio, const QSize& desiredSize)
{
    if (name.endsWith(".svg"))
        return getPixmapFromSvg(name, correctDevicePixelRatio, desiredSize);

    QPixmap result = getPixmapInternal(getDpiDependedName(name));

    if (correctDevicePixelRatio)
        correctPixelRatio(result);

    return result;
}

QPixmap Skin::getPixmapInternal(const QString& name)
{
    QPixmap pixmap;
    if (!QPixmapCache::find(name, &pixmap))
    {
        const auto fullPath = path(name);
        if (fullPath.isEmpty())
        {
            NX_ASSERT(name.contains(kHiDpiSuffix));
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

bool Skin::initSvgRenderer(const QString& name, QSvgRenderer& renderer) const
{
    QFile source(path(name));
    if (!source.open(QIODevice::ReadOnly))
    {
        NX_ASSERT(false, "Cannot load svg");
        return false;
    }

    if (!renderer.load(SvgIconColorer::colorized(source.readAll())))
    {
        NX_ASSERT(false, "Error while loading svg");
        return false;
    }

    return true;
}

QPixmap Skin::getPixmapFromSvg(const QString& name, bool correctDevicePixelRatio, const QSize& desiredSize)
{
    QPixmap pixmap;

    QSvgRenderer renderer;

    QSize size = desiredSize;
    if (!size.isValid())
    {
        if (!initSvgRenderer(name, renderer))
            return pixmap;

        size = renderer.defaultSize();

        if (!size.isValid())
        {
            NX_ASSERT(size.isValid(), nx::format("%1 doesn't define default size", name));
            return pixmap;
        }
    }

    const auto key = nx::format("%1:%2:%3:%4", name, correctDevicePixelRatio,
        size.width(), size.height());

    if (!QPixmapCache::find(key, &pixmap))
    {
        if (!renderer.isValid() && !initSvgRenderer(name, renderer))
            return pixmap;

        const QSize& correctedSize = correctDevicePixelRatio
            ? size * qApp->devicePixelRatio()
            : size;

        pixmap = QPixmap(correctedSize);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        renderer.render(&p);

        if (correctDevicePixelRatio)
            pixmap.setDevicePixelRatio(qApp->devicePixelRatio());

        QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
}

QString Skin::getDpiDependedName(const QString& name) const
{
    if (isHiDpi())
    {
        QFileInfo info(name);
        const auto suffix = info.completeSuffix();
        const auto newName = info.path() + '/' + info.completeBaseName() + kHiDpiSuffix
                + (suffix.isEmpty() ? QString() : '.' + info.suffix());

        const QString result = path(newName);
        if (!result.isEmpty())
            return result;
    }

    return path(name);
}

QStyle* Skin::newStyle()
{
    auto style = new Style();
    return new OldStyle(style);
}

QMovie* Skin::newMovie(const QString& name, QObject* parent)
{
    return new QMovie(getDpiDependedName(name), QByteArray(), parent);
}

QMovie* Skin::newMovie(const char* name, QObject* parent)
{
    return newMovie(QLatin1String(name), parent);
}

QSize Skin::maximumSize(const QIcon& icon, QIcon::Mode mode, QIcon::State state)
{
    int scale = isHiDpi() ? 2 : 1; //< we have only 1x and 2x scale icons
    return icon.actualSize(kHugeSize, mode, state) / scale;
}

QPixmap Skin::maximumSizePixmap(const QIcon& icon, QIcon::Mode mode,
    QIcon::State state, bool correctDevicePixelRatio)
{
    auto pixmap = icon.pixmap(kHugeSize, mode, state);
    if (correctDevicePixelRatio)
        correctPixelRatio(pixmap);
    return pixmap;
}

bool Skin::isHiDpi()
{
    return qApp->devicePixelRatio() > 1.0;
}

QPixmap Skin::colorize(const QPixmap& source, const QColor& color)
{
    QPixmap result(source);
    result.detach();

    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(QRect(QPoint(), result.size() / result.devicePixelRatio()), color);

    return result;
}

} // namespace nx::vms::client::desktop
