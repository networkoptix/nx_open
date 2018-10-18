#include "plugin_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

PluginEventModel::PluginEventModel(QObject* parent):
    QnCommonModuleAware(parent),
    QStandardItemModel(parent)
{
}

PluginEventModel::~PluginEventModel()
{
}

void PluginEventModel::buildFromList(const nx::vms::common::AnalyticsEngineResourceList& engines)
{
    beginResetModel();

    auto addItem = 
        [this](const QnUuid& id, const QString& name)
        {
            auto item = new QStandardItem(name);
            item->setData(qVariantFromValue(id), PluginIdRole);
            appendRow(item);
            return item;
        };

    clear();

    for (const auto& engine: engines)
    {
        addItem(engine->getId(), engine->getName());
    }

    endResetModel();
}

bool PluginEventModel::isValid() const
{
    return rowCount() > 0;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
