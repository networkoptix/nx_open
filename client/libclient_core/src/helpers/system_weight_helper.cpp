#include "system_weight_helper.h"

#include <nx/utils/system_utils.h>
#include <client_core/client_core_settings.h>

qreal helpers::calculateSystemWeight(qreal baseWeight, qint64 lastConnectedUtcMs)
{
    const std::chrono::milliseconds ms(lastConnectedUtcMs);
    const std::chrono::time_point<std::chrono::system_clock> timePoint(ms);
    return nx::utils::calculateSystemUsageFrequency(timePoint, baseWeight);
}

void helpers::updateWeightData(const QnUuid& localId)
{
    auto weightData = qnClientCoreSettings->localSystemWeightsData();
    const auto itWeightData = std::find_if(weightData.begin(), weightData.end(),
        [localId](const QnWeightData& data) { return data.localId == localId; });

    auto currentWeightData = (itWeightData == weightData.end()
        ? QnWeightData{localId, 0, QDateTime::currentMSecsSinceEpoch(), true}
        : *itWeightData);

    currentWeightData.weight = helpers::calculateSystemWeight(
        currentWeightData.weight, currentWeightData.lastConnectedUtcMs) + 1;
    currentWeightData.lastConnectedUtcMs = QDateTime::currentMSecsSinceEpoch();
    currentWeightData.realConnection = true;

    if (itWeightData == weightData.end())
        weightData.append(currentWeightData);
    else
        *itWeightData = currentWeightData;

    qnClientCoreSettings->setLocalSystemWeightsData(weightData);
}
