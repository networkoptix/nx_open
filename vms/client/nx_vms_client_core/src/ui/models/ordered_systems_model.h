#pragma once

#include <QFlags>

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

    virtual bool isHidden(const QModelIndex& dataIndex) const;

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
    Q_PROPERTY(int hiddenSystems READ hiddenSystems WRITE setHiddenSystems NOTIFY hiddenSystemsChanged)
    using base_type = QnSystemSortFilterListModel;

public:
    enum HiddenSystemsFlags {
        None = 0x0,
        Incompatible = 0x1,
        NotConnectableCloud = 0x2
    };
    Q_DECLARE_FLAGS(HiddenSystems, HiddenSystemsFlags)
    Q_FLAG(HiddenSystems)

    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

    virtual bool isForgotten(const QString& id) const override;

    virtual bool isHidden(const QModelIndex& dataIndex) const override;

    QString minimalVersion() const;

    void setMinimalVersion(const QString& minimalVersion);

    void setHiddenSystems(int flags);

    int hiddenSystems() const;

signals:
    void minimalVersionChanged();

    void hiddenSystemsChanged(int);

private:
    void handleWeightsChanged();

private:

    const QScopedPointer<QnSystemsModel> m_systemsModel;
    QFlags<HiddenSystemsFlags> m_systemsToHide;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnOrderedSystemsModel::HiddenSystems)
