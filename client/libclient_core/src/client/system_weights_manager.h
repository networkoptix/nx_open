
#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <client_core/local_connection_data.h>
#include <network/base_system_description.h>

class QnAbstractSystemsFinder;

typedef QHash<QString, QnWeightData> QnWeightsDataHash;

class QnSystemsWeightsManager : public QObject,
    public Singleton<QnSystemsWeightsManager>
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnSystemsWeightsManager();
    ~QnSystemsWeightsManager() = default;

    void setSystemsFinder(QnAbstractSystemsFinder* finder);

public:
    QnWeightsDataHash weights() const;

    qreal unknownSystemsWeight() const;

signals:
    void weightsChanged();

    void unknownSystemsWeightChanged();

private:
    void setUnknownSystemsWeight(qreal value);

    void addLocalWeightData(const QnWeightData& data);

    void processSystemDiscovered(const QnSystemDescriptionPtr& system);

    void resetWeights();

    void handleWeightsChanged();

    void updateWeightData();

private:
    QnAbstractSystemsFinder* m_finder;
    QnWeightsDataHash m_weights;
    qreal m_unknownSystemWeight;
};

#define qnSystemWeightsManager  QnSystemsWeightsManager::instance()
