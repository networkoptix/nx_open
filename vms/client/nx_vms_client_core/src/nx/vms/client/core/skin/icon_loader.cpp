// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "icon_loader.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtSvg/QSvgRenderer>

#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>

#include "icon_pixmap_accessor.h"
#include "skin.h"

namespace nx::vms::client::core {

namespace {

void decompose(const QString& path, QString* prefix, QString* suffix)
{
    QFileInfo info(path);
    *prefix = info.path() + '/' + info.baseName();
    *suffix = info.completeSuffix();
    if (!suffix->isEmpty())
        *suffix = '.' + *suffix;
}

class IconBuilder
{
public:
    IconBuilder(bool isHiDpi):
        m_isHiDpi(isHiDpi)
    {
    }

    void addSvg(const SvgIconColorer& colorer, QIcon::Mode mode, QIcon::State state)
    {
        const QByteArray data = colorer.makeIcon(mode);
        QSvgRenderer renderer;
        if (!renderer.load(data))
        {
            NX_ASSERT(false, "Error while loading svg");
            return;
        }

        const QSize size = m_isHiDpi ? renderer.defaultSize() * 2 : renderer.defaultSize();

        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        renderer.render(&p);
        add(pixmap, mode, state);
    }

    void add(const QPixmap& pixmap, QIcon::State state)
    {
        add(pixmap, QnIcon::Normal, state);
        add(pixmap, QnIcon::Disabled, state);
        add(pixmap, QnIcon::Active, state);
        add(pixmap, QnIcon::Selected, state);
        add(pixmap, QnIcon::Pressed, state);
    }

    void add(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state)
    {
        m_pixmaps[{mode, state, pixmap.size().width()}] = pixmap;
    }

    QIcon createIcon() const
    {
        QIcon icon;
        for (const auto& [key, image]: m_pixmaps)
        {
            auto [mode, state, _] = key;
            icon.addPixmap(image, mode, state);
        }
        return icon;
    }

private:
    const bool m_isHiDpi;
    // Storing as map to be able overwrite default values. Allowing to handle different sizes.
    using pixmap_key = std::tuple<QIcon::Mode, QIcon::State, int>;
    using container_type = std::map<pixmap_key, QPixmap>;
    container_type m_pixmaps;
};


QPixmap getImageFromSkin(Skin* skin, const QString& path)
{
    if (skin->hasFile(path))
        return skin->pixmap(path, false);
    return QPixmap();
}

void loadCustomIcons(
    Skin* skin,
    IconBuilder* builder,
    const QPixmap& baseImage,
    const QString& name,
    const QString& checkedName,
    const QnIcon::Suffixes* suffixes)
{
    QString prefix, extension, path;
    decompose(name, &prefix, &extension);

    for (auto suffix = suffixes->begin(); suffix != suffixes->end(); ++suffix)
    {
        if (suffix.value().isEmpty())
        {
            builder->add(baseImage, suffix.key(), QnIcon::Off);
        }
        else
        {
            path = prefix + "_" + suffix.value() + extension;
            auto image = getImageFromSkin(skin, path);
            if (!image.isNull())
                builder->add(image, suffix.key(), QnIcon::Off);
        }
    }

    decompose(checkedName.isEmpty()
        ? prefix + "_checked" + extension
        : checkedName,
        &prefix, &extension);

    path = prefix + extension;
    const auto baseCheckedImage = getImageFromSkin(skin, path);
    if (!baseCheckedImage.isNull())
        builder->add(baseCheckedImage, QnIcon::On);

    for (auto suffix = suffixes->begin(); suffix != suffixes->end(); ++suffix)
    {
        if (suffix.value().isEmpty())
        {
            if (!baseCheckedImage.isNull())
                builder->add(baseCheckedImage, suffix.key(), QnIcon::On);
        }
        else
        {
            path = prefix + "_" + suffix.value() + extension;
            auto image = getImageFromSkin(skin, path);
            if (!image.isNull())
                builder->add(image, suffix.key(), QnIcon::On);
        }
    }
}

} //namespace

const QnIcon::Suffixes IconLoader::kDefaultSuffixes({
    {QnIcon::Active,   "hovered"},
    {QnIcon::Disabled, "disabled"},
    {QnIcon::Selected, "selected"},
    {QnIcon::Pressed,  "pressed"},
    {QnIcon::Error,    "error"}
});

// -------------------------------------------------------------------------- //
// IconLoader
// -------------------------------------------------------------------------- //
IconLoader::IconLoader(QObject* parent):
    base_type(parent)
{
}

IconLoader::~IconLoader()
{
}

QIcon IconLoader::polish(const QIcon& icon)
{
    if (icon.isNull())
        return icon;

    if (m_cacheKeys.contains(icon.cacheKey()))
        return icon; /* Created by this loader => no need to polish. */

    QnIconPixmapAccessor accessor(&icon);
    if (accessor.size() == 0)
        return icon;

    QString pixmapName = accessor.name(0);
    if (!pixmapName.startsWith(":/skin"))
        return icon;

    /* Ok, that's an icon from :/skin or :/skin_dark */
    int index = pixmapName.indexOf('/', 2);
    if (index == -1)
        return icon;

    return load(pixmapName.mid(index + 1));
}

QIcon IconLoader::load(const QString& name,
    const QString& checkedName,
    const SvgIconColorer::ThemeSubstitutions& themeSubstitutions,
    const QnIcon::Suffixes* suffixes,
    const SvgIconColorer::IconSubstitutions& svgColorSubstitutions,
    const SvgIconColorer::IconSubstitutions& svgCheckedColorSubstitutions)
{
    static const QString kSeparator = "=^_^=";
    static const QString kHashSeparator = ">_<";

    QString key = name
        + kHashSeparator + svgColorSubstitutions.hash()
        + kSeparator + checkedName
        + kHashSeparator + svgCheckedColorSubstitutions.hash()
        + kSeparator + themeSubstitutions.hash();

    if (!suffixes)
    {
        key += kSeparator + "_default";
        suffixes = &kDefaultSuffixes;
    }
    else
    {
        key += kSeparator;
        for (auto suffix = suffixes->begin(); suffix != suffixes->end(); ++suffix)
            key += QString("_%1:%2").arg((int) suffix.key()).arg(suffix.value());
    }

    if (!m_iconByKey.contains(key))
    {
        const bool isSvg = name.endsWith(".svg");
        NX_ASSERT(isSvg || svgColorSubstitutions == SvgIconColorer::kDefaultIconSubstitutions,
            "Color substitutions cannot be specified for non-SVG icons.");

        const QIcon icon = isSvg
            ? loadSvgIconInternal(qnSkin, name, checkedName, svgColorSubstitutions, svgCheckedColorSubstitutions, themeSubstitutions)
            : loadPixmapIconInternal(qnSkin,  name, checkedName, suffixes);

        m_iconByKey.insert(key, icon);
        m_cacheKeys.insert(icon.cacheKey());
    }

    NX_ASSERT(m_iconByKey.contains(key));
    return m_iconByKey.value(key);
}

QIcon IconLoader::loadPixmapIconInternal(
    Skin* skin,
    const QString& name,
    const QString& checkedName,
    const QnIcon::Suffixes* suffixes)
{
    /* Create normal icon. */
    IconBuilder builder(Skin::isHiDpi());
    const auto basePixmap = skin->pixmap(name, false);
    builder.add(basePixmap, QnIcon::Normal, QnIcon::Off);
    loadCustomIcons(skin, &builder, basePixmap, name, checkedName, suffixes);

    return builder.createIcon();
}

QIcon IconLoader::loadSvgIconInternal(
    Skin* skin,
    const QString& name,
    const QString& checkedName,
    const SvgIconColorer::IconSubstitutions& substitutions,
    const SvgIconColorer::IconSubstitutions& checkedSubstitutions,
    const QMap<QIcon::Mode, SvgIconColorer::ThemeColorsRemapData>& themeSubstitutions)
{
    IconBuilder builder(Skin::isHiDpi());
    const auto basePath = skin->path(name);
    QFile source(basePath);
    if (!source.open(QIODevice::ReadOnly))
    {
        NX_ASSERT(false, "Cannot load icon %1", name);
        return QIcon();
    }

    const QByteArray baseData = source.readAll();
    const core::SvgIconColorer colorer(baseData, name, substitutions, themeSubstitutions);
    builder.addSvg(colorer, QnIcon::Normal, QIcon::Off);
    for (const auto& modeSubstitutions: substitutions.keys() + themeSubstitutions.keys())
        builder.addSvg(colorer, modeSubstitutions, QIcon::Off);

    if (!checkedName.isEmpty())
    {
        const auto checkedPath = skin->path(checkedName);
        QFile source(checkedPath);
        if (NX_ASSERT(source.open(QIODevice::ReadOnly), "Cannot load icon %1", checkedName))
        {
            const QByteArray checkedData = source.readAll();
            const auto actualSubstitutions =
                checkedSubstitutions.empty() ? substitutions : checkedSubstitutions;
            const core::SvgIconColorer colorer(
                checkedData, checkedName, actualSubstitutions, themeSubstitutions);
            builder.addSvg(colorer, QnIcon::Normal, QIcon::On);
            for (const auto& modeSubstitutions:
                actualSubstitutions.keys() + themeSubstitutions.keys())
            {
                builder.addSvg(colorer, modeSubstitutions, QIcon::On);
            }
        }
    }
    else if (!checkedSubstitutions.empty())
    {
        const core::SvgIconColorer checkedColorer(
            baseData, name, checkedSubstitutions, themeSubstitutions);
        for (const auto& modeSubstitutions:
            checkedSubstitutions.keys() + themeSubstitutions.keys())
        {
            builder.addSvg(checkedColorer, modeSubstitutions, QIcon::On);
        }
    }

    return builder.createIcon();
}

} // namespace nx::vms::client::core
