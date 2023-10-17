// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_event_list_model.h"

namespace nx::vms::client::desktop {

class SystemHealthListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    explicit SystemHealthListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~SystemHealthListModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

protected:
    virtual bool defaultAction(const QModelIndex& index) override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
