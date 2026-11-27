// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "organization_tree_model.h"

#include <nx/vms/client/core/cross_system/edge_db_data_loader.h>
#include <nx/vms/client/core/cross_system/edge_db_data_processor.h>
#include <nx/vms/client/core/network/edge_db_connection.h>
#include <nx/vms/client/mobile/resource_views/entity_resource_tree/organization_tree_entity_builder.h>

namespace nx::vms::client::mobile {

using Builder = entity_resource_tree::OrganizationTreeEntityBuilder;

struct OrganizationTreeModel::Private
{
    nx::Uuid organizationId;
    std::unique_ptr<Builder> builder = std::make_unique<Builder>();

    // TODO: #amalov Find a better place for connection & loader.
    std::unique_ptr<core::EdgeDbConnection> connection =
        std::make_unique<core::EdgeDbConnection>();
    std::unique_ptr<core::EdgeDbDataLoader> loader =
        std::make_unique<core::EdgeDbDataLoader>(connection.get());
    std::unique_ptr<core::EdgeDbDataProcessor> processor =
        std::make_unique<core::EdgeDbDataProcessor>(loader.get());
};

OrganizationTreeModel::OrganizationTreeModel(QObject* parent):
    base_type(parent),
    d(new Private{})
{
    connect(d->loader.get(),
        &core::EdgeDbDataLoader::failed,
        this,
        [this](const nx::network::rest::Result& error)
        {
            NX_WARNING(this,
                "Organization %1 data loading failed: %2",
                d->organizationId,
                error.errorString);
        });
}

OrganizationTreeModel::~OrganizationTreeModel()
{
}

void OrganizationTreeModel::registerQmlType()
{
    qmlRegisterType<OrganizationTreeModel>("nx.vms.client.mobile", 1, 0, "OrganizationTreeModel");
}

nx::Uuid OrganizationTreeModel::organizationId() const
{
    return d->organizationId;
}

void OrganizationTreeModel::setOrganizationId(nx::Uuid id)
{
    if (d->organizationId == id)
        return;

    d->organizationId = id;

    if (d->organizationId.isNull())
    {
        setRootEntity({});
    }
    else
    {
        d->loader->start(d->organizationId);
        setRootEntity(d->builder->createRootTreeEntity(d->organizationId));
    }

    emit organizationIdChanged();
}

void OrganizationTreeModel::onContextReady()
{
    // TODO: #amalov Fix model inheritance and assignment.
}

} // namespace nx::vms::client::mobile
