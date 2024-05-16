// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "invalid_resource_decorator_model.h"

#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>

namespace nx::vms::client::desktop {

InvalidResourceDecoratorModel::InvalidResourceDecoratorModel(
    const ResourceValidator& resourceValidator,
    QObject* parent)
    :
    base_type(parent),
    m_resourceValidator(resourceValidator)
{
}

QVariant InvalidResourceDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (role == ResourceDialogItemRole::IsValidResourceRole)
    {
        if (rowCount(index) > 0)
            return {};

        const auto resource = index.data(core::ResourceRole).value<QnResourcePtr>();

        if (resource.isNull())
            return {};

        return m_resourceValidator ? m_resourceValidator(resource) : true;
    }

    return base_type::data(index, role);
}

bool InvalidResourceDecoratorModel::hasInvalidResources(const QSet<QnResourcePtr>& resources) const
{
    return m_resourceValidator && std::any_of(std::cbegin(resources), std::cend(resources),
        [this](const QnResourcePtr& resource) { return !m_resourceValidator(resource); });
}

} // namespace nx::vms::client::desktop
