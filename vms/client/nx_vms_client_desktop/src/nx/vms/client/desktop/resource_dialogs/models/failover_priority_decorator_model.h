// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QIdentityProxyModel>

#include <core/resource/resource.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::client::desktop {

class FailoverPriorityDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;
    using FailoverPriorityMap = QHash<QnVirtualCameraResourcePtr, nx::vms::api::FailoverPriority>;

public:
    FailoverPriorityDecoratorModel(QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role) const override;

    FailoverPriorityMap failoverPriorityOverride() const;

    void setFailoverPriorityOverride(
        const QModelIndexList& indexes,
        nx::vms::api::FailoverPriority failoverPriority);

private:
    FailoverPriorityMap m_failoverPriorityOverride;
};

} // namespace nx::vms::client::desktop
