#include "plugin_event_model.h"

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::vms::client::desktop {
namespace ui {

void PluginEventModel::buildFromList(const nx::vms::common::AnalyticsEngineResourceList& engines)
{
    static const QString kAnyPlugin = tr("Any Plugin");

    const auto addItem =
        [this](const QnUuid& id, const QString& name)
        {
            auto item = new QStandardItem(name);
            item->setData(qVariantFromValue(id), PluginIdRole);
            appendRow(item);
        };

    beginResetModel();
    clear();
    addItem(QnUuid(), kAnyPlugin);
    for (const auto& engine: engines)
        addItem(engine->getId(), engine->getName());
    endResetModel();
}

bool PluginEventModel::isValid() const
{
    return rowCount() > 0;
}

} // namespace ui
} // namespace nx::vms::client::desktop
