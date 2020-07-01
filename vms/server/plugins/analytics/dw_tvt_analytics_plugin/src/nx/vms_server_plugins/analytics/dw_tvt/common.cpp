#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms_server_plugins::analytics::dw_tvt {

namespace {

bool containsSuperString(const QList<QString>& container, const QString& item) noexcept
{
    const auto modelIt = std::find_if(container.cbegin(), container.cend(),
        [&item](const auto& supportedModel)
        {
            return item.startsWith(supportedModel, Qt::CaseInsensitive);
        });

    return modelIt != container.cend();
}

} // namespace

bool EngineManifest::supportsModelCompletely(const QString& model) const noexcept
{
    if (supportedCameraModels.isEmpty() && partlySupportedCameraModels.isEmpty())
    {
        // No explicit camera model lists => any camera model is completely supported.
        return true;
    }
    return containsSuperString(supportedCameraModels, model);
}

bool EngineManifest::supportsModelPartly(const QString& model) const noexcept
{
    return containsSuperString(partlySupportedCameraModels, model);
}

bool EngineManifest::supportsModel(const QString& model) const noexcept
{
    return supportsModelCompletely(model) || supportsModelPartly(model);
}

QList<QString> EngineManifest::eventTypeIdListForModel(const QString& model) const noexcept
{
    QList<QString> result;
    result.reserve(eventTypes.size());
    const bool isCompletelySupported = supportsModelCompletely(model);

    for (const auto& eventType: eventTypes)
    {
        if (isCompletelySupported || !eventType.restricted)
            result.push_back(eventType.id);
    }
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (eq)(json), DwTvtEventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EngineManifest, (json), DwTvtEngineManifest_Fields, (brief, true))

} // nx::vms_server_plugins::analytics::dw_tvt
