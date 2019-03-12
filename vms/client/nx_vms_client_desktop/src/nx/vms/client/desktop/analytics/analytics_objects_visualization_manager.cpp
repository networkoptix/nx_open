#include "analytics_objects_visualization_manager.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/algorithm/same.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

namespace {

using Mode = AnalyticsObjectsVisualizationMode;

// Layout items with non-default ("always") mode.
using NonDefaultItems = QSet<QnUuid>;

NonDefaultItems filtered(NonDefaultItems source, QnResourcePool* resourcePool)
{
    QSet<QnUuid> existingItems;
    for (const auto& layout: resourcePool->getResources<QnLayoutResource>())
        existingItems.unite(layout->getItems().keys().toSet());

    source.intersect(existingItems);
    return source;
}

} // namespace

struct AnalyticsObjectsVisualizationManager::Private
{
    Mode mode(const QnLayoutItemIndex& index) const
    {
        return m_modes.contains(index.uuid())
            ? kNonDefaultMode
            : kDefaultMode;
    }

    void setMode(const QnLayoutItemIndex& index, Mode value)
    {
        NX_ASSERT(value != Mode::undefined);
        if (value == kDefaultMode)
            m_modes.remove(index.uuid());
        else
            m_modes.insert(index.uuid());
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

        m_modes = QnUbjson::deserialized<QSet<QnUuid>>(data.readAll());
        data.close();
    }

    void saveToFile(const QString& filename, QnResourcePool* resourcePool) const
    {
        QDir dir(cacheDirectory);
        if (!dir.mkpath(cacheDirectory))
            return;

        QFile data(dir.absoluteFilePath(filename));
        if (!data.open(QIODevice::WriteOnly))
            return;

        data.write(QnUbjson::serialized(filtered(m_modes, resourcePool)));
        data.close();
    }

    QString cacheDirectory;

private:
    // Set of layout item ids, which have non-default mode.
    NonDefaultItems m_modes;
};

AnalyticsObjectsVisualizationManager::AnalyticsObjectsVisualizationManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

AnalyticsObjectsVisualizationManager::~AnalyticsObjectsVisualizationManager()
{
}

Mode AnalyticsObjectsVisualizationManager::mode(const QnLayoutItemIndex& item) const
{
    return mode(QnLayoutItemIndexList() << item);
}

void AnalyticsObjectsVisualizationManager::setMode(const QnLayoutItemIndex& item, Mode value)
{
    setMode(QnLayoutItemIndexList() << item, value);
}

Mode AnalyticsObjectsVisualizationManager::mode(const QnLayoutItemIndexList& items) const
{
    auto value = Mode::undefined;
    if (nx::utils::algorithm::same(items.cbegin(), items.cend(),
        [this](const QnLayoutItemIndex& index)
        {
            return d->mode(index);
        },
        &value))
    {
        return value;
    }

    return Mode::undefined;
}

void AnalyticsObjectsVisualizationManager::setMode(const QnLayoutItemIndexList& items, Mode value)
{
    // undefined mode is not to be set.
    NX_ASSERT(value != Mode::undefined);
    if (value == Mode::undefined)
        return;

    for (const auto& item: items)
    {
        const auto oldMode = d->mode(item);
        d->setMode(item, value);
        if (oldMode != value)
            emit modeChanged(item, value);
    }
}

QString AnalyticsObjectsVisualizationManager::cacheDirectory() const
{
    return d->cacheDirectory;
}

void AnalyticsObjectsVisualizationManager::setCacheDirectory(const QString& value)
{
    d->cacheDirectory = value;
}

void AnalyticsObjectsVisualizationManager::switchLocalSystemId(const QnUuid& localSystemId)
{
    d->loadFromFile(localSystemId.toString());
}

void AnalyticsObjectsVisualizationManager::saveData(const QnUuid& localSystemId,
    QnResourcePool* resourcePool) const
{
    d->saveToFile(localSystemId.toString(), resourcePool);
}

} // namespace nx::vms::client::desktop
