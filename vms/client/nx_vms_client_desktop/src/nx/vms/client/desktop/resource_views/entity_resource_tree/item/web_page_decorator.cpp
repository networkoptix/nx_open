// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_page_decorator.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <core/resource/webpage_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/ui/image_providers/web_page_icon_cache.h>

namespace nx::vms::client::desktop::entity_resource_tree {

WebPageDecorator::WebPageDecorator(
    entity_item_model::AbstractItemPtr sourceItem,
    bool hasPowerUserPermissions)
    :
    m_sourceItem(std::move(sourceItem)),
    m_hasPowerUserPermissions(hasPowerUserPermissions)
{
    m_sourceItem->setDataChangedCallback(
        [this](const QVector<int>& roles) { notifyDataChanged(roles); });

    const auto webPage = getWebPage();

    if (!NX_ASSERT(webPage))
        return;

    if (const auto iconCache = appContext()->webPageIconCache())
    {
        connect(iconCache, &WebPageIconCache::iconChanged, this,
            [this, webPage](const QUrl& webPageUrl)
            {
                if (webPage->getUrl() == webPageUrl.toString())
                    notifyDataChanged({core::DecorationPathRole});
            });

        connect(webPage.get(), &QnWebPageResource::urlChanged,
            this, [this] { notifyDataChanged({core::DecorationPathRole}); });
    }
}

QVariant WebPageDecorator::data(int role) const
{
    switch (role)
    {
        case core::DecorationPathRole:
        {
            const auto webPage = getWebPage();
            if (!NX_ASSERT(webPage))
                return {};

            const auto iconPath = appContext()->webPageIconCache()->findPath(webPage->getUrl());
            return iconPath ? iconPath->toString() : QVariant{};
        }

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::resource);

        case Qn::ForceExtraInfoRole:
            return m_hasPowerUserPermissions;

        case Qn::ExtraInfoRole:
        {
            if (!m_hasPowerUserPermissions)
                return {};

            const auto webPage = getWebPage();
            if (!NX_ASSERT(webPage))
                return {};

            if (const auto parentServer = webPage->getParentResource())
                return "\u2B62 " + parentServer->getName(); //< Right arrow + server name.
        }
    }

    return m_sourceItem->data(role);
}

Qt::ItemFlags WebPageDecorator::flags() const
{
    auto result = m_sourceItem->flags();

    const auto resource = m_sourceItem->data(core::ResourceRole).value<QnResourcePtr>();
    if (!NX_ASSERT(!resource.isNull(), "Resource node expected"))
        return result;

    result.setFlag(Qt::ItemIsEditable, m_hasPowerUserPermissions);
    result.setFlag(Qt::ItemIsDragEnabled, true);
    result.setFlag(Qt::ItemIsDropEnabled, false);

    return result;
}

QnWebPageResourcePtr WebPageDecorator::getWebPage() const
{
    const auto resource = m_sourceItem->data(core::ResourceRole).value<QnResourcePtr>();
    return resource.dynamicCast<QnWebPageResource>();
}

} // namespace nx::vms::client::desktop::entity_resource_tree
