#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <watchers/cloud_status_watcher.h>
#include <client_core/local_connection_data.h>

class QTimer;

class QnOrderedSystemsModel : public QSortFilterProxyModel
{
    Q_OBJECT
    typedef QSortFilterProxyModel base_type;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

protected: // overrides
    virtual bool lessThan(const QModelIndex& left,
        const QModelIndex& right) const override;

    virtual bool filterAcceptsRow(int row,
        const QModelIndex &parent) const override;

private:
    void handleCloudSystemsChanged(const QnCloudSystemList& systems);

    void handleLocalWeightsChanged();

    void updateFinalWeights();

    void updateMaxRealWeight();

    qreal getWeight(const QModelIndex& modelIndex) const;

    typedef QHash<QString, QnWeightData> IdWeightDataHash;
    bool getWeightFromData(const QModelIndex& modelIndex,
        const IdWeightDataHash& data,
        qreal& weight) const;

private:

    QTimer* const m_updateTimer;
    IdWeightDataHash m_cloudWeights;
    IdWeightDataHash m_localWeights;
    IdWeightDataHash m_finalWeights;
    qreal m_maxRealWeight;
    mutable IdWeightDataHash m_newSystemWeights;
    mutable bool m_updatingWeights;
};