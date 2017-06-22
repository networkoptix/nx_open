#include "noptix_icon_loader.h"

#include <QtCore/QFileInfo>

#include "skin.h"
#include "icon_pixmap_accessor.h"

namespace {

void decompose(const QString& path, QString* prefix, QString* suffix)
{
    QFileInfo info(path);
    *prefix = info.path() + L'/' + info.baseName();
    *suffix = info.completeSuffix();
    if (!suffix->isEmpty())
        *suffix = L'.' + *suffix;
}

static const QnIcon::SuffixesList kDefaultSuffixes({
    {QnIcon::Active,   lit("hovered")},
    {QnIcon::Disabled, lit("disabled")},
    {QnIcon::Selected, lit("selected")},
    {QnIcon::Pressed,  lit("pressed")},
    {QnIcon::Error,    lit("error")}
});

} //namespace

// -------------------------------------------------------------------------- //
// QnIconBuilder
// -------------------------------------------------------------------------- //
class QnIconBuilder
{
public:
    void addPixmap(const QPixmap& pixmap, QIcon::Mode mode)
    {
        addPixmap(pixmap, mode, QnIcon::On);
        addPixmap(pixmap, mode, QnIcon::Off);
    }

    void addPixmap(const QPixmap& pixmap, QIcon::State state)
    {
        addPixmap(pixmap, QnIcon::Normal, state);
        addPixmap(pixmap, QnIcon::Disabled, state);
        addPixmap(pixmap, QnIcon::Active, state);
        addPixmap(pixmap, QnIcon::Selected, state);
        addPixmap(pixmap, QnIcon::Pressed, state);
    }

    void addPixmap(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state)
    {
        m_pixmaps[{mode, state}] = pixmap;
    }

    QPixmap pixmap(QIcon::Mode mode, QIcon::State state) const
    {
        return m_pixmaps[{mode, state}];
    }

    QIcon createIcon() const
    {
        QIcon icon(m_pixmaps.value({QIcon::Normal, QIcon::Off}));

        for (auto pos = m_pixmaps.begin(), end = m_pixmaps.end(); pos != end; pos++)
            icon.addPixmap(pos.value(), pos.key().first, pos.key().second);

        return icon;
    }

private:
    using container_type = QHash<QPair<QIcon::Mode, QIcon::State>, QPixmap>;
    container_type m_pixmaps;
};


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

    /* Ok, that's an icon from :/skin, :/skin_dark or :/skin_light. */
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
    QnSkin* skin = qnSkin;

    QString prefix, extension, path;

    decompose(name, &prefix, &extension);

    // TODO: #ynikitenkov Add multiple pixmaps mode (for 2x-3x..nx hidpi modes)

    /* Create normal icon. */
    QnIconBuilder builder;
    QPixmap basePixmap = skin->pixmap(name);
    builder.addPixmap(basePixmap, QnIcon::Normal, QnIcon::Off);

    for (auto suffix: *suffixes)
    {
        if (suffix.second.isEmpty())
        {
            builder.addPixmap(basePixmap, suffix.first, QnIcon::Off);
        }
        else
        {
            path = prefix + lit("_") + suffix.second + extension;
            if (skin->hasFile(path))
                builder.addPixmap(skin->pixmap(path), suffix.first, QnIcon::Off);
        }
    }

    decompose(checkedName.isEmpty()
        ? prefix + lit("_checked") + extension
        : checkedName,
        &prefix, &extension);

    path = prefix + extension;
    QPixmap baseCheckedPixmap;

    if (skin->hasFile(path))
    {
        baseCheckedPixmap = skin->pixmap(path);
        builder.addPixmap(baseCheckedPixmap, QnIcon::On);
    }

    for (auto suffix: *suffixes)
    {
        if (suffix.second.isEmpty())
        {
            if (!baseCheckedPixmap.isNull())
                builder.addPixmap(baseCheckedPixmap, suffix.first, QnIcon::On);
        }
        else
        {
            path = prefix + lit("_") + suffix.second + extension;
            if (skin->hasFile(path))
                builder.addPixmap(skin->pixmap(path), suffix.first, QnIcon::On);
        }
    }

    QIcon icon = builder.createIcon();
    m_iconByKey.insert(key, icon);
    m_cacheKeys.insert(icon.cacheKey());
}
