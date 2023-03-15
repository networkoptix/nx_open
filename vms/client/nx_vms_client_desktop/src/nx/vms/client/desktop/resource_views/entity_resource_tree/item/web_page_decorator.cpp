// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_page_decorator.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <core/resource/webpage_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace nx::vms::client::desktop::entity_resource_tree {

WebPageDecorator::WebPageDecorator(
    entity_item_model::AbstractItemPtr sourceItem,
    bool hasAdminPermissions)
    :
    m_sourceItem(std::move(sourceItem)),
    m_hasAdminPermissions(hasAdminPermissions)
{
    m_sourceItem->setDataChangedCallback(
        [this](const QVector<int>& roles) { notifyDataChanged(roles); });
}

QVariant WebPageDecorator::data(int role) const
{
    switch (role)
    {
        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::resource);

        case Qn::ForceExtraInfoRole:
            return m_hasAdminPermissions;

        case Qn::ExtraInfoRole:
        {
            if (!m_hasAdminPermissions)
                return {};

            const auto resource = m_sourceItem->data(Qn::ResourceRole).value<QnResourcePtr>();
            const auto webPage = resource.dynamicCast<QnWebPageResource>();

            if (!NX_ASSERT(webPage))
                return {};

            if (const auto parentServer = webPage->getParentResource())
                return "\U0001F816 " + parentServer->getName(); //< Right arrow + server name.
        }
    }

    return m_sourceItem->data(role);
}

Qt::ItemFlags WebPageDecorator::flags() const
{
    auto result = m_sourceItem->flags();

    const auto resource = m_sourceItem->data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!NX_ASSERT(!resource.isNull(), "Resource node expected"))
        return result;

    result.setFlag(Qt::ItemIsEditable, m_hasAdminPermissions);
    result.setFlag(Qt::ItemIsDragEnabled, true);
    result.setFlag(Qt::ItemIsDropEnabled, false);

    return result;
}

} // namespace nx::vms::client::desktop::entity_resource_tree
