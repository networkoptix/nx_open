#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <client/system_weights_manager.h>

class QnOrderedSystemsModel: public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString minimalVersion READ minimalVersion WRITE setMinimalVersion NOTIFY minimalVersionChanged)
    typedef QSortFilterProxyModel base_type;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

    QString minimalVersion() const;

    void setMinimalVersion(const QString& minimalVersion);

signals:
    void minimalVersionChanged();

protected: // overrides
    virtual bool lessThan(const QModelIndex& left,
        const QModelIndex& right) const override;

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void handleWeightsChanged();

    qreal getWeight(const QModelIndex& modelIndex) const;

    bool getWeightFromData(const QModelIndex& modelIndex,
        qreal& weight) const;

    void softInvalidate();

private:
    QnWeightsDataHash m_weights;
    qreal m_unknownSystemsWeight;
};
