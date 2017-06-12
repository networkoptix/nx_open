#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <client_core/local_connection_data.h>
#include <network/base_system_description.h>

class QTimer;
class QnAbstractSystemsFinder;

using nx::client::core::WeightData;

typedef QHash<QString, WeightData> QnWeightsDataHash;

class QnSystemsWeightsManager:
    public QObject,
    public Singleton<QnSystemsWeightsManager>
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnSystemsWeightsManager();
    ~QnSystemsWeightsManager() = default;

public:
    QnWeightsDataHash weights() const;

    qreal unknownSystemsWeight() const;

    void setWeight(
        const QnUuid& localSystemId,
        qreal weight);

signals:
    void weightsChanged();

    void unknownSystemsWeightChanged();

private:
    void setSystemsFinder(QnAbstractSystemsFinder* finder);

    void setUnknownSystemsWeight(qreal value);

    void addLocalWeightData(const WeightData& data);

    void processSystemDiscovered(const QnSystemDescriptionPtr& system);

    void resetWeights();

    void afterBaseWeightsUpdated();

    void handleSourceWeightsChanged();

private:
    QTimer* const m_updateTimer;

    QnAbstractSystemsFinder* m_finder;
    QnWeightsDataHash m_baseWeights;
    QnWeightsDataHash m_updatedWeights;
    qreal m_unknownSystemWeight;
};

#define qnSystemWeightsManager  QnSystemsWeightsManager::instance()
