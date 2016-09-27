
#include "system_weight_updater.h"

#include <QtCore/QTimer>

#include <client_core/client_core_settings.h>

QnSystemsWeightUpadater::QnSystemsWeightUpadater(QObject* parent) :
    base_type(parent),
    m_updateTimer(new QTimer(this))
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueType)
        {
            if (valueType == QnClientCoreSettings::RecentLocalConnections)
                updateWeights();
        });

    updateWeights();

    static const int kUpdatePeriod = 60 * 10 * 1000;   //< 10 minutes

    connect(m_updateTimer, &QTimer::timeout, this, &QnSystemsWeightUpadater::updateWeights);
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(kUpdatePeriod);
    m_updateTimer->start();
}

void QnSystemsWeightUpadater::updateWeights()
{
    auto connections = qnClientCoreSettings->recentLocalConnections();
    const auto oldWeights = connections.getWeights();

    QnLocalConnectionDataList::WeightHash newWeights;
    for (auto& connection : connections)
    {
        connection.weight = connection.calcWeight();
        newWeights.insert(connection.systemId, connection.weight);
    }

    if (newWeights == oldWeights)
        return;

    qnClientCoreSettings->setRecentLocalConnections(connections);
    qnClientCoreSettings->save();
}