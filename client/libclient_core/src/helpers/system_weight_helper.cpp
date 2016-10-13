
#include "system_weight_helper.h"

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
