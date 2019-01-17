#include "noptix_icon_loader.h"

#include <QtCore/QFileInfo>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtSvg/QSvgRenderer>

#include "skin.h"
#include "icon_pixmap_accessor.h"
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>

namespace {

static const QnIcon::SuffixesList kDefaultSuffixes({
    {QnIcon::Active,   "hovered"},
    {QnIcon::Disabled, "disabled"},
    {QnIcon::Selected, "selected"},
    {QnIcon::Pressed,  "pressed"},
    {QnIcon::Error,    "error"}
});

static constexpr QSize kBaseIconSize(20, 20);

void decompose(const QString& path, QString* prefix, QString* suffix)
{
    QFileInfo info(path);
    *prefix = info.path() + L'/' + info.baseName();
    *suffix = info.completeSuffix();
    if (!suffix->isEmpty())
        *suffix = L'.' + *suffix;
}

class IconBuilder
{
public:
    IconBuilder(bool isHiDpi):
        m_isHiDpi(isHiDpi)
    {
    }

    void addSvg(const QByteArray& data, QIcon::Mode mode, QIcon::State state)
    {
        QSvgRenderer renderer;
        if (!renderer.load(data))
        {
            NX_ASSERT(false, "Error while loading svg");
            return;
        }

        const QSize size = m_isHiDpi ? kBaseIconSize * 2 : kBaseIconSize;

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


QPixmap getImageFromSkin(QnSkin* skin, const QString& path)
{
    if (skin->hasFile(path))
        return skin->pixmap(path, false);
    return QPixmap();
}

void loadCustomIcons(
    QnSkin* skin,
    IconBuilder* builder,
    const QPixmap& baseImage,
    const QString& name,
    const QString& checkedName,
    const QnIcon::SuffixesList* suffixes)
{
    QString prefix, extension, path;
    decompose(name, &prefix, &extension);

    for (auto suffix: *suffixes)
    {
        if (suffix.second.isEmpty())
        {
            builder->add(baseImage, suffix.first, QnIcon::Off);
        }
        else
        {
            path = prefix + lit("_") + suffix.second + extension;
            auto image = getImageFromSkin(skin, path);
            if (!image.isNull())
                builder->add(image, suffix.first, QnIcon::Off);
        }
    }

    decompose(checkedName.isEmpty()
        ? prefix + lit("_checked") + extension
        : checkedName,
        &prefix, &extension);

    path = prefix + extension;
    const auto baseCheckedImage = getImageFromSkin(skin, path);
    if (!baseCheckedImage.isNull())
        builder->add(baseCheckedImage, QnIcon::On);

    for (auto suffix: *suffixes)
    {
        if (suffix.second.isEmpty())
        {
            if (!baseCheckedImage.isNull())
                builder->add(baseCheckedImage, suffix.first, QnIcon::On);
        }
        else
        {
            path = prefix + lit("_") + suffix.second + extension;
            auto image = getImageFromSkin(skin, path);
            if (!image.isNull())
                builder->add(image, suffix.first, QnIcon::On);
        }
    }
}

} //namespace

// -------------------------------------------------------------------------- //
// QnNoptixIconLoader
// -------------------------------------------------------------------------- //
QnNoptixIconLoader::QnNoptixIconLoader(QObject* parent):
    base_type(parent)
{
}

QnNoptixIconLoader::~QnNoptixIconLoader()
{
}

QIcon QnNoptixIconLoader::polish(const QIcon& icon)
{
    if (icon.isNull())
        return icon;

    if (m_cacheKeys.contains(icon.cacheKey()))
        return icon; /* Created by this loader => no need to polish. */

    QnIconPixmapAccessor accessor(&icon);
    if (accessor.size() == 0)
        return icon;

    QString pixmapName = accessor.name(0);
    if (!pixmapName.startsWith(lit(":/skin")))
        return icon;

    /* Ok, that's an icon from :/skin or :/skin_dark */
    int index = pixmapName.indexOf(L'/', 2);
    if (index == -1)
        return icon;

    return load(pixmapName.mid(index + 1));
}

QIcon QnNoptixIconLoader::load(
    const QString& name,
    const QString& checkedName,
    const QnIcon::SuffixesList* suffixes)
{
    static const QString kSeparator = lit("=^_^=");

    QString key = name + kSeparator + checkedName;
    if (!suffixes)
    {
        key += kSeparator + lit("_default");
        suffixes = &kDefaultSuffixes;
    }
    else
    {
        key += kSeparator;
        for (auto suffix: *suffixes)
            key += lit("_%1:%2").arg(static_cast<int>(suffix.first)).arg(suffix.second);
    }

    if (!m_iconByKey.contains(key))
        loadIconInternal(key, name, checkedName, suffixes);

    NX_ASSERT(m_iconByKey.contains(key));
    return m_iconByKey.value(key);
}

void QnNoptixIconLoader::loadIconInternal(
    const QString& key,
    const QString& name,
    const QString& checkedName,
    const QnIcon::SuffixesList* suffixes)
{
    const auto icon = name.endsWith(".svg")
        ? loadSvgIconInternal(qnSkin, name, checkedName, suffixes)
        : loadPixmapIconInternal(qnSkin,  name, checkedName, suffixes);

    m_iconByKey.insert(key, icon);
    m_cacheKeys.insert(icon.cacheKey());
}

QIcon QnNoptixIconLoader::loadPixmapIconInternal(
    QnSkin* skin,
    const QString& name,
    const QString& checkedName,
    const QnIcon::SuffixesList* suffixes)
{
    /* Create normal icon. */
    IconBuilder builder(QnSkin::isHiDpi());
    const auto basePixmap = skin->pixmap(name, false);
    builder.add(basePixmap, QnIcon::Normal, QnIcon::Off);
    loadCustomIcons(skin, &builder, basePixmap, name, checkedName, suffixes);

    return builder.createIcon();
}

QIcon QnNoptixIconLoader::loadSvgIconInternal(
    QnSkin* skin,
    const QString& name,
    const QString& checkedName,
    const QnIcon::SuffixesList* suffixes)
{
    const auto basePath = skin->path(name);
    QFile source(basePath);
    if (!source.open(QIODevice::ReadOnly))
    {
        NX_ASSERT(false, "Cannot load svg icon");
        return QIcon();
    }

    const QByteArray baseData = source.readAll();

    IconBuilder builder(QnSkin::isHiDpi());
    builder.addSvg(baseData, QnIcon::Normal, QnIcon::Off);

    auto color =
        [theme = nx::vms::client::desktop::colorTheme()](const QString& name)
        {
            return theme->color(name).name();
        };
    const QString primaryColor = "#A5B7C0"; //< Value of light10 in default customization.
    const QString secondaryColor = "#E1E7EA"; //< Value of light4 in default customization.

    const auto colorized =
        [&](const QString& primary, const QString& secondary)
        {
            return nx::utils::replaceStrings(baseData, {
                {primaryColor, color(primary)},
                {secondaryColor, color(secondary)},
                }).toLatin1();
        };

    builder.addSvg(colorized("dark14", "dark17"), QnIcon::Disabled, QnIcon::Off);
    builder.addSvg(colorized("light4", "light1"), QnIcon::Selected, QnIcon::Off);
    builder.addSvg(colorized("brand_core", "brand_l2"), QnIcon::Active, QnIcon::Off);
    builder.addSvg(colorized("red_l2", "red_l3"), QnIcon::Error, QnIcon::Off);

    // This can be enabled if we will need to override some icons
    //loadCustomIcons(skin, &builder, basePath, name, checkedName, suffixes);

    return builder.createIcon();
}
