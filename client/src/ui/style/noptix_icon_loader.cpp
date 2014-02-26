#include "noptix_icon_loader.h"

#include "skin.h"
#include "icon.h"
#include "icon_pixmap_accessor.h"

void decompose(const QString &path, QString *prefix, QString *suffix) {
    QFileInfo info(path);
    *prefix = info.path() + L'/' + info.baseName();
    *suffix = info.completeSuffix();
    if(!suffix->isEmpty())
        *suffix = L'.' + *suffix;
}

// -------------------------------------------------------------------------- //
// QnIconBuilder
// -------------------------------------------------------------------------- //
class QnIconBuilder {
    typedef QHash<QPair<QIcon::Mode, QIcon::State>, QPixmap> container_type;

public:
    void addPixmap(const QPixmap &pixmap, QIcon::Mode mode) {
        addPixmap(pixmap, mode, QnIcon::On);
        addPixmap(pixmap, mode, QnIcon::Off);
    }

    void addPixmap(const QPixmap &pixmap, QIcon::State state) {
        addPixmap(pixmap, QnIcon::Normal,   state);
        addPixmap(pixmap, QnIcon::Disabled, state);
        addPixmap(pixmap, QnIcon::Active,   state);
        addPixmap(pixmap, QnIcon::Selected, state);
        addPixmap(pixmap, QnIcon::Pressed, state);
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


// -------------------------------------------------------------------------- //
// QnNoptixIconLoader
// -------------------------------------------------------------------------- //
QnNoptixIconLoader::QnNoptixIconLoader(QObject *parent):
    base_type(parent)
{}

QnNoptixIconLoader::~QnNoptixIconLoader() {
    return;
}

QIcon QnNoptixIconLoader::polish(const QIcon &icon) {
    if(icon.isNull())
        return icon;

    if(m_cacheKeys.contains(icon.cacheKey()))
        return icon; /* Created by this loader => no need to polish. */

    QnIconPixmapAccessor accessor(&icon);
    if(accessor.size() == 0)
        return icon; 

    QString pixmapName = accessor.name(0);
    if(!pixmapName.startsWith(lit(":/skin")))
        return icon;

    /* Ok, that's an icon from :/skin, :/skin_dark or :/skin_light. */
    int index = pixmapName.indexOf(L'/', 2);
    if(index == -1)
        return icon;

    return load(pixmapName.mid(index + 1));
}

QIcon QnNoptixIconLoader::load(const QString &name, const QString &checkedName) {
    QString key = name + lit("=^_^=") + checkedName;
    if(m_iconByKey.contains(key))
        return m_iconByKey.value(key);

    QnSkin *skin = qnSkin;

    QString prefix, suffix, path;

    decompose(name, &prefix, &suffix);

    /* Create normal icon. */
    QnIconBuilder builder;
    builder.addPixmap(skin->pixmap(name), QnIcon::Normal, QnIcon::Off);

    path = prefix + lit("_hovered") + suffix;
    if(qnSkin->hasFile(path)) 
        builder.addPixmap(skin->pixmap(path), QnIcon::Active);

    path = prefix + lit("_disabled") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Disabled);

    path = prefix + lit("_selected") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Selected);

    path = prefix + lit("_pressed") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Pressed);

    decompose(checkedName.isEmpty() ? prefix + lit("_checked") + suffix : checkedName, &prefix, &suffix);

    path = prefix + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::On);

    path = prefix + lit("_hovered") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Active, QnIcon::On);

    path = prefix + lit("_disabled") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Disabled, QnIcon::On);

    path = prefix + lit("_selected") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Selected, QnIcon::On);

    path = prefix + lit("_pressed") + suffix;
    if(skin->hasFile(path))
        builder.addPixmap(skin->pixmap(path), QnIcon::Pressed, QnIcon::On);

    QIcon icon = builder.createIcon();
    m_iconByKey.insert(key, icon);
    m_cacheKeys.insert(icon.cacheKey());
    return icon;
}
