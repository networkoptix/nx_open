// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <client/client_globals.h>
#include <nx/utils/data_structures/keyed_list.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class LocalNotificationsListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public WindowContextAware
{
   Q_OBJECT
   using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    LocalNotificationsListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~LocalNotificationsListModel() override = default;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual bool setData(const QModelIndex& index, const QVariant& value,
        int role = Qn::DefaultNotificationRole) override;

    virtual bool removeRows(
        int row, int count, const QModelIndex& parent = QModelIndex()) override;

private:
    nx::utils::KeyList<QnUuid> m_notifications;
};

} // nx::vms::client::desktop
