// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
class RemoteAccessModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

    Q_OBJECT
public:
    enum class Roles
    {
        name = Qt::UserRole + 1,
        port,
        login,
        password,
    };

    explicit RemoteAccessModel(QObject* parent = nullptr);
    virtual ~RemoteAccessModel() override;

    void setServer(const ServerResourcePtr& server);

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
