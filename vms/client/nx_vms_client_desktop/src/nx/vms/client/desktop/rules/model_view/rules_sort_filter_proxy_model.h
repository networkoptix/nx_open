// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop::rules {

class RulesTableModel;

class RulesSortFilterProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    explicit RulesSortFilterProxyModel(QObject* parent = nullptr);

    /** Returns rule ids for the given rows. */
    Q_INVOKABLE QnUuidList getRuleIds(const QList<int>& rows) const;

    static void registerQmlType();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    RulesTableModel* m_rulesTableModel{nullptr};
};

} // namespace nx::vms::client::desktop::rules
