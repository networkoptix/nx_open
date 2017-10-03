#include "radass_resource_manager.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_support.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/algorithm/same.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RadassMode)

using ModeByIdHash = QHash<QnUuid, RadassMode>;

namespace
{

ModeByIdHash filtered(ModeByIdHash source, QnResourcePool* resourcePool)
{
    QSet<QnUuid> existingItems;
    for (const auto& layout: resourcePool->getResources<QnLayoutResource>())
        existingItems.unite(layout->getItems().keys().toSet());

    const auto storedItems = source.keys().toSet();

    auto removedItems = storedItems - existingItems;
    for (const auto& item: removedItems)
        source.remove(item);

    return source;
}

} // namespace

struct RadassResourceManager::Private
{
    RadassMode mode(const QnLayoutItemIndex& index) const
    {
        return m_modes.value(index.uuid(), RadassMode::Auto);
    }

    void setMode(const QnLayoutItemIndex& index, RadassMode value)
    {
        NX_ASSERT(value != RadassMode::Custom);
        if (value == RadassMode::Auto)
            m_modes.remove(index.uuid());
        else
            m_modes[index.uuid()] = value;
    }

    void loadFromFile(const QString& filename)
    {
        m_modes.clear();

        QDir dir(cacheDirectory);
        if (!dir.exists())
            return;

        QFile data(dir.absoluteFilePath(filename));
        if (!data.open(QIODevice::ReadOnly))
            return;

        m_modes = QnUbjson::deserialized<QHash<QnUuid, RadassMode>>(data.readAll());
        data.close();
    }

    void saveToFile(const QString& filename, QnResourcePool* resourcePool) const
    {
        QDir dir(cacheDirectory);
        dir.mkpath(cacheDirectory);

        QFile data(dir.absoluteFilePath(filename));
        if (!data.open(QIODevice::WriteOnly))
            return;

        data.write(QnUbjson::serialized(filtered(m_modes, resourcePool)));
        data.close();
    }

    QString cacheDirectory;

private:
    // Mode by layout item uuid.
    ModeByIdHash m_modes;
};

RadassResourceManager::RadassResourceManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

RadassResourceManager::~RadassResourceManager()
{
}

RadassMode RadassResourceManager::mode(const QnLayoutResourcePtr& layout) const
{
    if (!layout)
        return RadassMode::Auto;

    QnLayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << QnLayoutItemIndex(layout, item.uuid);

    return mode(items);
}

void RadassResourceManager::setMode(const QnLayoutResourcePtr& layout, RadassMode value)
{
    if (!layout)
        return;

    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    QnLayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << QnLayoutItemIndex(layout, item.uuid);

    setMode(items, value);
}

RadassMode RadassResourceManager::mode(const QnLayoutItemIndex& item) const
{
    return mode(QnLayoutItemIndexList() << item);
}

void RadassResourceManager::setMode(const QnLayoutItemIndex& item, RadassMode value)
{
    setMode(QnLayoutItemIndexList() << item, value);
}

RadassMode RadassResourceManager::mode(const QnLayoutItemIndexList& items) const
{
    QnLayoutItemIndexList validItems;
    for (const auto& item: items)
    {
        if (isRadassSupported(item))
            validItems.push_back(item);
    }

    auto value = RadassMode::Auto;
    if (nx::utils::algorithm::same(validItems.cbegin(), validItems.cend(),
        [this](const QnLayoutItemIndex& index)
        {
            return d->mode(index);
        },
        &value))
    {
        return value;
    }

    return RadassMode::Custom;
}

void RadassResourceManager::setMode(const QnLayoutItemIndexList& items, RadassMode value)
{
    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    for (const auto& item: items)
    {
        if (!isRadassSupported(item))
            continue;

        auto oldMode = d->mode(item);
        d->setMode(item, value);
        if (oldMode != value)
            emit modeChanged(item, value);
    }
}

QString RadassResourceManager::cacheDirectory() const
{
    return d->cacheDirectory;
}

void RadassResourceManager::setCacheDirectory(const QString& value)
{
    d->cacheDirectory = value;
}

void RadassResourceManager::switchLocalSystemId(const QnUuid& localSystemId)
{
    d->loadFromFile(localSystemId.toString());
}

void RadassResourceManager::saveData(const QnUuid& localSystemId,
    QnResourcePool* resourcePool) const
{
    d->saveToFile(localSystemId.toString(), resourcePool);
}

} // namespace desktop
} // namespace client
} // namespace nx
