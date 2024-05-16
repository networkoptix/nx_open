// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/models/abstract_event_list_model.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class SystemHealthListModel: public core::AbstractEventListModel, public WindowContextAware
{
    Q_OBJECT
    using base_type = core::AbstractEventListModel;

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
