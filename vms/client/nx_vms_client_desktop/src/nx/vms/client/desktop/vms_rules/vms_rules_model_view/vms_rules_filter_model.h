// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop {
namespace vms_rules {

class VmsRulesFilterModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;
    Q_OBJECT

public:
    VmsRulesFilterModel(QObject* parent = nullptr);

    void setFilterString(const QString& string);

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_filterString;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
