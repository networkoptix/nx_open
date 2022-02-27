// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QFlags>

#include <ui/models/systems_model.h>
#include <ui/models/sort_filter_list_model.h>
#include <client/system_weights_manager.h>
#include <nx/vms/client/core/enums.h>

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

    using base_type = QnSystemSortFilterListModel;

    using CoreEnums = nx::vms::client::core::Enums;

public:
    QnOrderedSystemsModel(QObject* parent = nullptr);

    virtual ~QnOrderedSystemsModel() = default;

    virtual bool isForgotten(const QString& id) const override;

    virtual bool isHidden(const QModelIndex& dataIndex) const override;

    static Q_INVOKABLE int searchRoleId();

private:
    void handleWeightsChanged();

private:
    QnSystemsModel* const m_systemsModel;
};
