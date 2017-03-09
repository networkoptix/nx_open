#pragma once

#include <ui/models/sorting_proxy_model.h>
#include <client/system_weights_manager.h>

class QnSystemsModel;
class QnOrderedSystemsModel: public QnSortingProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString minimalVersion READ minimalVersion WRITE setMinimalVersion NOTIFY minimalVersionChanged)
    using base_type = QnSortingProxyModel;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

    QString minimalVersion() const;

    void setMinimalVersion(const QString& minimalVersion);

signals:
    void minimalVersionChanged();

private: // Prediactes
    bool lessPredicate(const QModelIndex& left, const QModelIndex& right) const;

    bool filterPredicate(const QModelIndex& index) const;

private:
    void handleWeightsChanged();

    qreal getWeight(const QModelIndex& modelIndex) const;

    bool getWeightFromData(const QModelIndex& modelIndex,
        qreal& weight) const;

private:
    QnSystemsModel * const m_source;
    QnWeightsDataHash m_weights;
    qreal m_unknownSystemsWeight;
};
