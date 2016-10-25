
#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <client_core/local_connection_data.h>
#include <network/base_system_description.h>

class QnAbstractSystemsFinder;

typedef QHash<QString, QnWeightData> QnWeightsDataHash;
class QnSystemsWeightsManager : public Singleton<QnSystemsWeightsManager>,
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnWeightDataList weights READ weights NOTIFY weightsChanged)
    Q_PROPERTY(qreal unknownSystemsWeight READ unknownSystemsWeight NOTIFY unknownSystemsWeight)

public:
    QnSystemsWeightsManager();
    ~QnSystemsWeightsManager() = default;

    void setSystemsFinder(QnAbstractSystemsFinder* finder);

public:
    QnWeightsDataHash weights() const;

signals:
    void weightsChanged();

private:
    void addLocalWeightData(const QnWeightData& data);

    void processSystemDiscovered(const QnSystemDescriptionPtr& system);

    void setWeights(const QnWeightsDataHash& weights);

    void resetWeights();

private:
    QnAbstractSystemsFinder* m_finder;
    QnWeightsDataHash m_weights;
    qreal m_unknownSystemWeight;
};