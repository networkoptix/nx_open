#pragma once

#include <ui/models/systems_model.h>
#include <ui/models/sort_filter_list_model.h>
#include <client/system_weights_manager.h>

class QnSystemSortFilterListModel: public QnSortFilterListModel
{
    Q_OBJECT
    using base_type = QnSortFilterListModel;

public:
    QnSystemSortFilterListModel(QObject* parent);

    bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

    virtual bool isForgotten(const QString& id) const;

    void setWeights(const QnWeightsDataHash& weights, qreal unknownSystemsWeight);

private:
    bool lessThan(
        const QModelIndex& left,
        const QModelIndex& right) const override;

    WeightData getWeight(const QModelIndex& modelIndex) const;

private:
    QnWeightsDataHash m_weights;
    qreal m_unknownSystemsWeight;
};

class QnOrderedSystemsModel: public QnSystemSortFilterListModel
{
    Q_OBJECT
    Q_PROPERTY(QString minimalVersion READ minimalVersion WRITE setMinimalVersion NOTIFY minimalVersionChanged)
    using base_type = QnSystemSortFilterListModel;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

    virtual bool isForgotten(const QString& id) const override;

    QString minimalVersion() const;

    void setMinimalVersion(const QString& minimalVersion);

signals:
    void minimalVersionChanged();

private:
    void handleWeightsChanged();

private:

    const QScopedPointer<QnSystemsModel> m_systemsModel;
};
