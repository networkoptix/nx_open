// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "radass_resource_manager.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/utils/algorithm/same.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_support.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>

namespace nx::vms::client::desktop {

using ModeByIdHash = QHash<nx::Uuid, RadassMode>;

NX_REFLECTION_INSTRUMENT_ENUM(RadassMode, Auto, High, Low)

static const RadassMode kDefaultResolution = nx::reflect::fromString(ini().defaultResolution,
    RadassMode::Auto);

namespace {

ModeByIdHash filtered(ModeByIdHash source, QnResourcePool* resourcePool)
{
    QSet<nx::Uuid> existingItems;
    for (const auto& layout: resourcePool->getResources<QnLayoutResource>())
        existingItems.unite(nx::utils::toQSet(layout->getItems().keys()));

    const auto storedItems = nx::utils::toQSet(source.keys());

    auto removedItems = storedItems - existingItems;
    for (const auto& item: removedItems)
        source.remove(item);

    return source;
}

} // namespace

struct RadassResourceManager::Private
{
    RadassMode mode(const LayoutItemIndex& index) const
    {
        return m_modes.value(index.uuid(), kDefaultResolution);
    }

    void setMode(const LayoutItemIndex& index, RadassMode value)
    {
        NX_ASSERT(value != RadassMode::Custom);
        if (value == RadassMode::Auto && kDefaultResolution == RadassMode::Auto)
        {
            m_modes.remove(index.uuid());
        }
        else
        {
            m_modes[index.uuid()] = value;
        }
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

        m_modes = QnUbjson::deserialized<QHash<nx::Uuid, RadassMode>>(data.readAll());
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

RadassMode RadassResourceManager::mode(const LayoutResourcePtr& layout) const
{
    if (!layout)
        return RadassMode::Auto;

    LayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << LayoutItemIndex(layout, item.uuid);

    return mode(items);
}

void RadassResourceManager::setMode(const LayoutResourcePtr& layout, RadassMode value)
{
    if (!layout)
        return;

    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    LayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << LayoutItemIndex(layout, item.uuid);

    setMode(items, value);
}

RadassMode RadassResourceManager::mode(const LayoutItemIndex& item) const
{
    return mode(LayoutItemIndexList() << item);
}

void RadassResourceManager::setMode(const LayoutItemIndex& item, RadassMode value)
{
    setMode(LayoutItemIndexList() << item, value);
}

RadassMode RadassResourceManager::mode(const LayoutItemIndexList& items) const
{
    LayoutItemIndexList validItems;
    for (const auto& item: items)
    {
        if (isRadassSupported(item))
            validItems.push_back(item);
    }

    auto value = RadassMode::Auto;
    if (nx::utils::algorithm::same(validItems.cbegin(), validItems.cend(),
        [this](const LayoutItemIndex& index)
        {
            return d->mode(index);
        },
        &value))
    {
        return value;
    }

    return RadassMode::Custom;
}

void RadassResourceManager::setMode(const LayoutItemIndexList& items, RadassMode value)
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

void RadassResourceManager::switchLocalSystemId(const nx::Uuid& localSystemId)
{
    d->loadFromFile(localSystemId.toString());
}

void RadassResourceManager::saveData(const nx::Uuid& localSystemId,
    QnResourcePool* resourcePool) const
{
    d->saveToFile(localSystemId.toString(), resourcePool);
}

} // namespace nx::vms::client::desktop
