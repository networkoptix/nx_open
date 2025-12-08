// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_item.h"

#include <QtCore/QVariant>

#include <client/client_globals.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/resource/server.h>

using namespace nx::vms::client::core;
using namespace nx::vms::client::desktop;

namespace {

QVariant resourceHelpTopic(const QnResourcePtr& resource)
{
    if (resource.isNull())
        return QVariant();

    if (resource->hasFlags(Qn::server))
        return HelpTopic::Id::MainWindow_Tree_Servers;

    if (resource->hasFlags(Qn::io_module))
        return HelpTopic::Id::IOModules;

    if (resource->hasFlags(Qn::live_cam))
        return HelpTopic::Id::MainWindow_Tree_Camera;

    if (resource->hasFlags(Qn::layout))
    {
        auto layout = resource.staticCast<QnLayoutResource>();
        if (layout->isFile())
            return HelpTopic::Id::MainWindow_Tree_MultiVideo;
        return HelpTopic::Id::MainWindow_Tree_Layouts;
    }

    if (resource->hasFlags(Qn::user))
        return HelpTopic::Id::MainWindow_Tree_Users;

    if (resource->hasFlags(Qn::web_page))
        return HelpTopic::Id::MainWindow_Tree_WebPage;

    if (resource->hasFlags(Qn::videowall))
        return HelpTopic::Id::Videowall;

    if (resource->hasFlags(Qn::local))
        return HelpTopic::Id::MainWindow_Tree_Exported;

    return QVariant();
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

ResourceItem::ResourceItem(const QnResourcePtr& resource):
    base_type(resource),
    m_helpTopic(resourceHelpTopic(resource))
{
    // Item will not send notifications for the never accessed data roles. This workaround is
    // needed to guarantee that all notifications for the roles listed below will be delivered.
    // TODO: #vbreus Roles that trigger model layout changes should be declared in a special way.
    data(Qn::GlobalPermissionsRole);
}

QVariant ResourceItem::data(int role) const
{
    switch (role)
    {
        case Qn::HelpTopicIdRole:
            return helpTopic();
    }
    return base_type::data(role);
}

void ResourceItem::initResourceIconKeyNotifications() const
{
    base_type::initResourceIconKeyNotifications();

    const auto discardIconCache =
        [this] { discardCache(m_resourceIconKeyCache, { core::ResourceIconKeyRole }); };

    if (const auto server = m_resource.staticCast<ServerResource>())
    {
        m_connectionsGuard.add(server->connect(server.get(),
            &ServerResource::compatibilityChanged,
            discardIconCache));
        m_connectionsGuard.add(server->connect(server.get(),
            &ServerResource::isDetachedChanged,
            discardIconCache));
    }
}

QVariant ResourceItem::helpTopic() const
{
    return m_helpTopic;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
