// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "skin.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QPluginLoader>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <nx/utils/log/log.h>

#include "icon_loader.h"
#include "svg_icon_colorer.h"

namespace nx::vms::client::core {

static Skin* s_instance = nullptr;

namespace {

static const QSize kHugeSize(100000, 100000);
static const auto kHiDpiSuffix = "@2x";

void correctPixelRatio(QPixmap& pixmap)
{
    if (Skin::isHiDpi())
        pixmap.setDevicePixelRatio(2.0);
}

} // namespace

Skin::Skin(const QStringList& paths, QObject* parent):
    QObject(parent)
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

    init(paths);
}


Skin::~Skin()
{
    if (s_instance == this)
        s_instance = nullptr;
}

Skin* Skin::instance()
{
    return s_instance;
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
    const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
    const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions,
    const SvgIconColorer::ThemeSubstitutions& svgThemeSubstitutions)
{
    return m_iconLoader->load(name,
        checkedName,
        svgThemeSubstitutions,
        suffixes,
        svgColorSubstitutions,
        svgCheckedColorSubstitutions);
}

QIcon Skin::icon(const char* name, const char* checkedName)
{
    return icon(QLatin1String(name), QLatin1String(checkedName));
}

QIcon Skin::icon(const QIcon& icon)
{
    return m_iconLoader->polish(icon);
}

QIcon Skin::icon(const QString& name,
    const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
    const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions)
{
    return m_iconLoader->load(name,
        QString(),
        {},
        nullptr,
        svgColorSubstitutions,
        svgCheckedColorSubstitutions);
}

QIcon Skin::icon(const QString& name,
    const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
    const QString& checkedName,
    const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions)
{
    return m_iconLoader->load(
        name, checkedName, {}, nullptr, svgColorSubstitutions, svgCheckedColorSubstitutions);
}

QIcon Skin::icon(const QString& name,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions)
{
    return m_iconLoader->load(name, QString(), themeSubstitutions);
}

QIcon Skin::icon(const QString& name,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions,
    const QString& checkedName)
{
    return m_iconLoader->load(name,
        checkedName,
        themeSubstitutions);
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
            NX_ASSERT(!pixmap.isNull(), "Can't find icon %1", fullPath);
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
        NX_ASSERT(false, "Cannot load svg with name " + name);
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

} // namespace nx::vms::client::core
