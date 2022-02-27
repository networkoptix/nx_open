// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop {

class InvalidResourceFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;

public:
    InvalidResourceFilterModel(QObject* parent = nullptr);

    bool showInvalidResources() const;
    void setShowInvalidResources(bool show);

protected:
    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    bool m_showInvalidResources = false;
};

} // namespace nx::vms::client::desktop
