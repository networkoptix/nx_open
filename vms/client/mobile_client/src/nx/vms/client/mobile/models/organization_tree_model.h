// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_tree_model.h"

namespace nx::vms::client::mobile {

class OrganizationTreeModel: public ResourceTreeModel
{
    Q_OBJECT
    using base_type = ResourceTreeModel;

    Q_PROPERTY(nx::Uuid organizationId READ organizationId WRITE setOrganizationId
        NOTIFY organizationIdChanged)

public:
    OrganizationTreeModel(QObject* parent = nullptr);
    virtual ~OrganizationTreeModel() override;

    static void registerQmlType();

    nx::Uuid organizationId() const;
    void setOrganizationId(nx::Uuid id);

signals:
    void organizationIdChanged();

protected:
    virtual void onContextReady() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
