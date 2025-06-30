// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "skin.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QPluginLoader>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/svg_loader.h>

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

ColorizedIconDeclaration::Storage* ColorizedIconDeclaration::storage()
{
    static auto ptr = new Storage(); //< To guarantee that it's initialized on time.
    return ptr;
}

ColorizedIconDeclaration::ColorizedIconDeclaration(
    const QString& sourceFile,
    const QString& iconPath,
    const SvgIconColorer::IconSubstitutions& substitutions)
    :
    m_sourceFile(sourceFile),
    m_iconPath(iconPath),
    m_substitutions(substitutions)
{
    storage()->append(this);
}

ColorizedIconDeclaration::ColorizedIconDeclaration(
    const QString& sourceFile,
    const QString& iconPath,
    const SvgIconColorer::IconSubstitutions& substitutions,
    const QString& checkedIconPath,
    const SvgIconColorer::IconSubstitutions& checkedSubstitutions)
    :
    m_sourceFile(sourceFile),
    m_iconPath(iconPath),
    m_checkedIconPath(checkedIconPath),
    m_substitutions(substitutions),
    m_checkedSubstitutions(checkedSubstitutions)
{
    storage()->append(this);
}

ColorizedIconDeclaration::ColorizedIconDeclaration(
    const QString& sourceFile,
    const QString& iconPath,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions)
    :
    m_sourceFile(sourceFile),
    m_iconPath(iconPath),
    m_themeSubstitutions(themeSubstitutions)
{
    storage()->append(this);
}

ColorizedIconDeclaration::ColorizedIconDeclaration(
    const QString& sourceFile,
    const QString& iconPath,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions,
    const QString& checkedIconPath,
    const SvgIconColorer::ThemeSubstitutions& checkedThemeSubstitutions)
    :
    m_sourceFile(sourceFile),
    m_iconPath(iconPath),
    m_checkedIconPath(checkedIconPath),
    m_themeSubstitutions(themeSubstitutions),

    m_checkedThemeSubstitutions(checkedThemeSubstitutions)
{
    storage()->append(this);
}

ColorizedIconDeclaration::~ColorizedIconDeclaration()
{
    storage()->removeOne(this);
}

QString ColorizedIconDeclaration::sourceFile() const
{
    return m_sourceFile;
}

QString ColorizedIconDeclaration::iconPath() const
{
    return m_iconPath;
}

QString ColorizedIconDeclaration::checkedIconPath() const
{
    return m_checkedIconPath;
}

SvgIconColorer::IconSubstitutions ColorizedIconDeclaration::substitutions() const
{
    return m_substitutions;
}

SvgIconColorer::IconSubstitutions ColorizedIconDeclaration::checkedSubstitutions() const
{
    return m_checkedSubstitutions;
}

SvgIconColorer::ThemeSubstitutions ColorizedIconDeclaration::themeSubstitutions() const
{
    return m_themeSubstitutions;
}

SvgIconColorer::ThemeSubstitutions ColorizedIconDeclaration::checkedThemeSubstitutions() const
{
    return m_checkedThemeSubstitutions;
}

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
        * dpr * dpr); // dpi-scaled
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
    const SvgIconColorer::ThemeSubstitutions& svgThemeSubstitutions,
    const SvgIconColorer::ThemeSubstitutions& svgCheckedThemeSubstitutions)
{
    return m_iconLoader->load(name,
        checkedName,
        svgThemeSubstitutions,
        svgCheckedThemeSubstitutions,
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

QIcon Skin::icon(const ColorizedIconDeclaration& decl)
{
    return m_iconLoader->load(
        decl.iconPath(),
        decl.checkedIconPath(),
        decl.themeSubstitutions(),
        decl.checkedThemeSubstitutions(),
        nullptr,
        decl.substitutions(),
        decl.checkedSubstitutions());
}

QIcon Skin::icon(const QString& name,
    const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
    const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions)
{
    return m_iconLoader->load(name,
        QString(),
        {},
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
        name, checkedName, {}, {}, nullptr, svgColorSubstitutions, svgCheckedColorSubstitutions);
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

QIcon Skin::icon(const QString& name,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions,
    const SvgIconColorer::ThemeSubstitutions& checkedThemeSubstitutions)
{
    return m_iconLoader->load(name,
        QString(),
        themeSubstitutions, checkedThemeSubstitutions);
}

bool Skin::addGeneratedPixmap(const QString& name, const QPixmap& pixmap)
{
    if (m_externalPixmaps.contains(name))
        return false;

    m_externalPixmaps[name] = pixmap;
    return true;
}

QPixmap Skin::pixmap(
    const QString& name,
    bool correctDevicePixelRatio,
    const QSize& desiredSize)
{
    const auto [pixmap, error] = pixmapInternal(name, correctDevicePixelRatio, desiredSize);

    NX_ASSERT(error.isEmpty(), error);

    return pixmap;
}

std::pair<QPixmap, QString> Skin::createPixmap(const QString& name)
{
    const QString dpiDependedName = getDpiDependedName(name);

    QPixmap pixmap;
    QString error;
    if (!QPixmapCache::find(dpiDependedName, &pixmap))
    {
        if (m_externalPixmaps.contains(dpiDependedName))
        {
            pixmap = m_externalPixmaps[dpiDependedName];
        }
        else if (const auto fullPath = path(dpiDependedName);
            fullPath.isEmpty())
        {
            if (!dpiDependedName.contains(kHiDpiSuffix))
            {
                error = nx::format("Name does not include \"%1\": %2, %3",
                    kHiDpiSuffix, dpiDependedName, name);
            }
        }
        else
        {
            pixmap = QPixmap::fromImage(QImage(fullPath), Qt::OrderedDither | Qt::OrderedAlphaDither);
            pixmap.setDevicePixelRatio(1); // Force to use not scaled images
            if (pixmap.isNull())
                error = nx::format("Can't find icon: %1, %2", fullPath, name);
        }
        QPixmapCache::insert(dpiDependedName, pixmap);
    }
    return {pixmap, error};
}

std::pair<QPixmap, QString> Skin::pixmapInternal(
    const QString& name,
    bool correctDevicePixelRatio,
    const QSize& desiredSize)
{
    if (name.endsWith(".svg"))
        return createPixmapFromSvg(name, correctDevicePixelRatio, desiredSize);

    std::pair<QPixmap, QString> result = createPixmap(name);
    if (correctDevicePixelRatio)
        correctPixelRatio(result.first);

    return result;
}

QString Skin::initSvgRenderer(
    const QString& name,
    QSvgRenderer& renderer) const
{
    QFile source(path(name));
    if (!source.open(QIODevice::ReadOnly))
        return "Cannot open svg file: " + name;

    if (!renderer.load(SvgIconColorer::colorized(source.readAll())))
        return "Error while loading svg: " + name;

    return {};
}

std::pair<QPixmap, QString> Skin::createPixmapFromSvg(
    const QString& name,
    bool correctDevicePixelRatio,
    const QSize& desiredSize)
{
    QPixmap pixmap;

    QSvgRenderer renderer;

    QSize size = desiredSize;
    if (!size.isValid())
    {
        QString rendererError = initSvgRenderer(name, renderer);
        if (!rendererError.isEmpty())
            return {pixmap, rendererError};

        size = renderer.defaultSize();

        if (!size.isValid())
            return {pixmap, nx::format("%1 doesn't define default size", name)};
    }

    const auto key = nx::format("%1:%2:%3:%4", name, correctDevicePixelRatio,
        size.width(), size.height());

    if (!QPixmapCache::find(key, &pixmap))
    {
        QString rendererError = initSvgRenderer(name, renderer);
        if (!renderer.isValid() && !rendererError.isEmpty())
            return {pixmap, rendererError};

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

    return {pixmap, QString()};
}

QString Skin::getDpiDependedName(const QString& name) const
{
    if (name.endsWith(".gen"))
        return name;

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

QPixmap Skin::colorizedPixmap(
    const QString& pathAndParams,
    const QSize& desiredPhysicalSize,
    QColor primaryColor,
    qreal devicePixelRatio)
{
    QUrl url(pathAndParams);
    if (!NX_ASSERT(url.isValid() && url.isRelative(),
        "Invalid skin image relative URL: \"%1\"", pathAndParams))
    {
        return {};
    }

    QMap<QString /*source class name*/, QString /*target color name*/> colorMap;
    for (const auto& [className, colorName]: QUrlQuery(url.query()).queryItems())
        colorMap[className] = colorName;

    static const QString kPrimaryKey = "primary";
    if (primaryColor.isValid())
    {
        // Set/reset primary color to the specified value.
        colorMap[kPrimaryKey] = primaryColor.name();
    }

    const auto path = url.path();
    if (path.endsWith(".svg"))
    {
        const auto fullPath = this->path(path);
        return NX_ASSERT(!fullPath.isEmpty(), "\"%1\" not found.", path)
            ? loadSvgImage(fullPath, colorMap, desiredPhysicalSize, devicePixelRatio)
            : QPixmap{};
    }

    // Only optional primary channel is allowed for non-SVG images.
    NX_ASSERT(colorMap.empty() || (colorMap.size() == 1 && colorMap.contains(kPrimaryKey)));

    auto [pixmap, error] = pixmapInternal(path, /*correctDevicePixelRatio*/ true);

    static const QString kIgnoreErrorsFragment = "ignoreErrors";
    const bool ignoreErrors = url.hasFragment() && url.fragment() == kIgnoreErrorsFragment;

    if (!error.isEmpty())
    {
        if (ignoreErrors)
            NX_WARNING(this, error);
        else
            NX_ASSERT(false, error);
    }
    else if (pixmap.isNull())
    {
        const QString emptyPixmapError = nx::format("Empty pixmap: %1", pathAndParams);
        if (ignoreErrors)
            NX_WARNING(this, emptyPixmapError);
        else
            NX_ASSERT(false, emptyPixmapError);
    }

    if (pixmap.isNull())
        return pixmap;

    if (colorMap.contains(kPrimaryKey))
    {
        const auto colorName = colorMap.value(kPrimaryKey);
        const auto targetColor = colorTheme()->safeColor(colorName);

        NX_ASSERT(targetColor.isValid(), "Invalid color \"%1\" for image \"%2\"", colorName, path);
        if (targetColor.isValid())
            pixmap = colorize(pixmap, targetColor);
    }

    if (!desiredPhysicalSize.isEmpty())
        pixmap = pixmap.scaled(desiredPhysicalSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}

} // namespace nx::vms::client::core
