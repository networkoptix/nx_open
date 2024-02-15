// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event_model.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::vms::client::desktop::ui {

void PluginDiagnosticEventModel::filterByCameras(
    nx::vms::common::AnalyticsEngineResourceList engines,
    const QnVirtualCameraResourceList& cameras)
{
    using namespace nx::vms::common;

    if (!cameras.isEmpty())
    {
        QSet<nx::Uuid> compatibleEngineIds = cameras[0]->compatibleAnalyticsEngines();
        for (int i = 1; i < cameras.size(); ++i)
            compatibleEngineIds.intersect(cameras[i]->compatibleAnalyticsEngines());

        engines = engines.filtered<AnalyticsEngineResource>(
            [&compatibleEngineIds](const AnalyticsEngineResourcePtr& engine)
            {
                return compatibleEngineIds.contains(engine->getId());
            });
    }

    static const QString kAnyPlugin = tr("Any Plugin");

    const auto addItem =
        [this](const nx::Uuid& id, const QString& name)
        {
            auto item = new QStandardItem(name);
            item->setData(QVariant::fromValue(id), PluginIdRole);
            appendRow(item);
        };

    beginResetModel();
    clear();

    if (cameras.isEmpty())
        addItem(nx::Uuid(), kAnyPlugin); //< By default should be the first item in the model.

    // All the engines should be sorted by the name.
    std::sort(
        engines.begin(),
        engines.end(),
        [](const AnalyticsEngineResourcePtr& l, const AnalyticsEngineResourcePtr& r)
        {
            return l->getName() < r->getName();
        });

    for (const auto& engine: engines)
        addItem(engine->getId(), engine->getName());

    endResetModel();
}

bool PluginDiagnosticEventModel::isValid() const
{
    return rowCount() > 0;
}

} // namespace nx::vms::client::desktop::ui
