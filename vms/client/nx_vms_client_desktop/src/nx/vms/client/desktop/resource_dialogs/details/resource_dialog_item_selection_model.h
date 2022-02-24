// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QItemSelectionModel>

namespace nx::vms::client::desktop {

class ResourceDialogItemSelectionModel: public QItemSelectionModel
{
    Q_OBJECT
    using base_type = QItemSelectionModel;

public:
    ResourceDialogItemSelectionModel(QAbstractItemModel* model, QObject* parent);

    virtual void select(
        const QItemSelection& selection,
        QItemSelectionModel::SelectionFlags command) override;
};

} // namespace nx::vms::client::desktop
