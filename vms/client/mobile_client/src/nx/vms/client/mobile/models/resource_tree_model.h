// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::vms::client::mobile {

class ResourceTreeModel: public QIdentityProxyModel,
    public WindowContextAware,
    public core::ContextFromQmlHandler
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    ResourceTreeModel(QObject* parent = nullptr);
    virtual ~ResourceTreeModel() override;

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE QModelIndex resourceIndex(QnResource* resource) const;

private:
    virtual void onContextReady() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
