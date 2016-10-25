#include "system_weight_helper.h"

#include <client_core/client_core_settings.h>

qreal helpers::calculateSystemWeight(qreal baseWeight, qint64 lastConnectedUtcMs)
{
    static const auto getDays =
        [](qint64 utcMsSinceEpoch)
        {
            const qint64 kMsInDay = 60 * 60 * 24 * 1000;
            return utcMsSinceEpoch / kMsInDay;
        };

    const auto currentTime = QDateTime::currentMSecsSinceEpoch();
    const auto penalty = (getDays(currentTime) - getDays(lastConnectedUtcMs)) / 30.0;
    return std::max<qreal>((1.0 - penalty) * baseWeight, 0);

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
