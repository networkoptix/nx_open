#include "noptix_icon_loader.h"

#include "skin.h"
#include "icon.h"
#include "icon_pixmap_accessor.h"

namespace
{
    void decompose(const QString& path, QString* prefix, QString* suffix)
    {
        QFileInfo info(path);
        *prefix = info.path() + L'/' + info.baseName();
        *suffix = info.completeSuffix();
        if (!suffix->isEmpty())
            *suffix = L'.' + *suffix;
    }
}

// -------------------------------------------------------------------------- //
// QnIconBuilder
// -------------------------------------------------------------------------- //
class QnIconBuilder
{
    typedef QHash<QPair<QIcon::Mode, QIcon::State>, QPixmap> container_type;

public:
    void addPixmap(const QPixmap& pixmap, QIcon::Mode mode)
    {
        addPixmap(pixmap, mode, QnIcon::On);
        addPixmap(pixmap, mode, QnIcon::Off);
    }

    void addPixmap(const QPixmap& pixmap, QIcon::State state)
    {
        addPixmap(pixmap, QnIcon::Normal,   state);
        addPixmap(pixmap, QnIcon::Disabled, state);
        addPixmap(pixmap, QnIcon::Active,   state);
        addPixmap(pixmap, QnIcon::Selected, state);
        addPixmap(pixmap, QnIcon::Pressed,  state);
    }

    void addPixmap(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state)
    {
        m_pixmaps[qMakePair(mode, state)] = pixmap;
    }

    QIcon createIcon() const
    {
        QIcon icon(m_pixmaps.value(qMakePair(QIcon::Normal, QIcon::Off)));

        for (container_type::const_iterator pos = m_pixmaps.begin(), end = m_pixmaps.end(); pos != end; pos++)
            icon.addPixmap(pos.value(), pos.key().first, pos.key().second);

        return icon;
    }

private:
    container_type m_pixmaps;
};


// -------------------------------------------------------------------------- //
// QnNoptixIconLoader
// -------------------------------------------------------------------------- //
QnNoptixIconLoader::QnNoptixIconLoader(QObject* parent): base_type(parent)
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

QIcon QnNoptixIconLoader::load(const QString& name, const QString& checkedName, int numModes, const QPair<QIcon::Mode, QString>* modes)
{
    static const QVector<QPair<QIcon::Mode, QString> > kDefaultModes({
        qMakePair(QnIcon::Active,   lit("hovered")),
        qMakePair(QnIcon::Disabled, lit("disabled")),
        qMakePair(QnIcon::Selected, lit("selected")),
        qMakePair(QnIcon::Pressed,  lit("pressed"))
    });

    static const QString kSeparator = lit("=^_^=");

    QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);
    if (!guiApp)
        return QIcon();

    QString key = name + kSeparator + checkedName;
    if (numModes < 0)
    {
        key += kSeparator + lit("_default");
        numModes = kDefaultModes.size();
        modes = &kDefaultModes.front();
    }
    else
    {
        key += kSeparator;
        for (int i = 0; i < numModes; ++i)
            key += lit("_%1:%2").arg(static_cast<int>(modes[i].first)).arg(modes[i].second);
    }

    if (m_iconByKey.contains(key))
        return m_iconByKey.value(key);

    QnSkin* skin = qnSkin;

    QString prefix, suffix, path;

    decompose(name, &prefix, &suffix);

    /* Create normal icon. */
    QnIconBuilder builder;
    builder.addPixmap(skin->pixmap(name), QnIcon::Normal, QnIcon::Off);

    for (int i = 0; i < numModes; ++i)
    {
        path = prefix + lit("_") + modes[i].second + suffix;
        if (skin->hasFile(path))
            builder.addPixmap(skin->pixmap(path), modes[i].first, QnIcon::Off);
    }

    decompose(checkedName.isEmpty() ? prefix + lit("_checked") + suffix : checkedName, &prefix, &suffix);

    /* Create checked icon. */
    path = prefix + suffix;
    if (skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::On);

    for (int i = 0; i < numModes; ++i)
    {
        path = prefix + lit("_") + modes[i].second + suffix;
        if (skin->hasFile(path))
            builder.addPixmap(skin->pixmap(path), modes[i].first, QnIcon::On);
    }

    QIcon icon = builder.createIcon();
    m_iconByKey.insert(key, icon);
    m_cacheKeys.insert(icon.cacheKey());
    return icon;
}
