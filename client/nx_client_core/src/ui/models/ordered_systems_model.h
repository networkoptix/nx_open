#pragma once

#include <ui/models/sort_filter_list_model.h>
#include <client/system_weights_manager.h>

class QnOrderedSystemsModel: public QnSortFilterListModel
{
    Q_OBJECT
    Q_PROPERTY(QString minimalVersion READ minimalVersion WRITE setMinimalVersion NOTIFY minimalVersionChanged)
    using base_type = QnSortFilterListModel;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

    QString minimalVersion() const;

    void setMinimalVersion(const QString& minimalVersion);

signals:
    void minimalVersionChanged();

private: // overrides
    bool lessThan(
        const QModelIndex& left,
        const QModelIndex& right) const override;

private:
    void handleWeightsChanged();

    qreal getWeight(const QModelIndex& modelIndex) const;

private:
    QnWeightsDataHash m_weights;
    qreal m_unknownSystemsWeight;
};
