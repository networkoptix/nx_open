#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <watchers/cloud_status_watcher.h>
#include <client_core/local_connection_data.h>

class QTimer;

class QnOrderedSystemsModel : public QSortFilterProxyModel
{
    Q_OBJECT
    typedef QSortFilterProxyModel base_type;

    Q_PROPERTY(int sourceRowsCount READ sourceRowsCount NOTIFY sourceRowsCountChanged)

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

public: // properties
    int sourceRowsCount() const;

signals:
    void sourceRowsCountChanged();

protected: // overrides
    virtual bool lessThan(const QModelIndex& left,
        const QModelIndex& right) const override;

    virtual bool filterAcceptsRow(int row,
        const QModelIndex &parent) const override;

private:
    void handleCloudSystemsChanged();

    void handleLocalWeightsChanged();

    void updateFinalWeights();

    qreal getWeight(const QModelIndex& modelIndex) const;

    void setSourceRowsCount(int value);

    void updateSourceRowsCount();

private:
    typedef QHash<QString, QnWeightData> IdWeightDataHash;

    QTimer* const m_updateTimer;
    IdWeightDataHash m_cloudWeights;
    IdWeightDataHash m_localWeights;
    IdWeightDataHash m_finalWeights;

    mutable IdWeightDataHash m_newSystemWeights;
    mutable bool m_updatingWeights;

    int m_sourceRowsCount;
};