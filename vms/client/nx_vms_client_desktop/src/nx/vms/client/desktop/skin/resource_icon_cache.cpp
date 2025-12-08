// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_cache.h"

#include <client/client_globals.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/server.h>

namespace {

using namespace nx::vms::client::core;

bool isCompatibleServer(const QnResourcePtr& resource)
{
    auto server = resource.dynamicCast<nx::vms::client::desktop::ServerResource>();
    return server && server->isCompatible();
}

bool isDetachedServer(const QnResourcePtr& resource)
{
    auto server = resource.dynamicCast<nx::vms::client::desktop::ServerResource>();
    return server && server->isDetached();
}

bool isVideoWallReviewLayout(const LayoutResourcePtr& layout)
{
    return layout && layout->data().contains(Qn::VideoWallResourceRole);
}

} // namspace

namespace nx::vms::client::desktop {

ResourceIconCache::ResourceIconCache(QObject* parent):
    base_type(parent)
{
    m_legacyIconsPaths.insert(CrossSystemStatus | Control, "20x20/Outline/loaders.svg.gen"); //< The Control uses to describe loading state.
    m_legacyIconsPaths.insert(CrossSystemStatus | Offline, "cloud/cloud_20_disabled.png");

    // TODO: #vbutkevich must be removed when m_legacyIconsPaths will be changed to svg icons.
    for (const auto& [flag, path]: std::as_const(m_legacyIconsPaths).asKeyValueRange())
    {
        IconWithDescription value;
        const auto substitutions =
            flag.testFlag(Offline) ? treeThemeOfflineSubstitutions() : treeThemeSubstitutions();
        if (path.endsWith(".gen"))
        {
            value.icon = qnSkin->pixmap(path);
        }
        else
        {
            value.icon = loadIcon(path, substitutions);
            value.iconDescription = ColorizedIconDeclaration(
                "resource_icon_cache.cpp", path, substitutions);
        }
        m_cache.insert(flag, value);
    }
}

ResourceIconCache::~ResourceIconCache()
{
}

QString ResourceIconCache::iconPath(Key key) const
{
    const auto legacyIconPath = iconPathFromLegacyIcons(key);
    if (!legacyIconPath.isEmpty())
        return legacyIconPath;

    return base_type::iconPath(key);
}

QString ResourceIconCache::iconPathFromLegacyIcons(Key key) const
{
    auto it = m_legacyIconsPaths.find(key);
    if (it != m_legacyIconsPaths.cend())
        return *it;

    it = m_legacyIconsPaths.find(key & (TypeMask | StatusMask));
    if (it != m_legacyIconsPaths.cend())
        return *it;

    it = m_legacyIconsPaths.find(key & TypeMask);
    return it != m_legacyIconsPaths.cend() ? (*it) : QString{};
}

ResourceIconCache::Key ResourceIconCache::key(const QnResourcePtr& resource) const
{
    const auto iconKey = base_type::key(resource);

    ResourceIconCache::Key iconType = iconKey & ResourceIconCache::TypeMask;
    ResourceIconCache::Key iconStatus = iconKey & ResourceIconCache::StatusMask;
    ResourceIconCache::Key iconFlags = iconKey & ResourceIconCache::AdditionalFlagsMask;

    if (const auto layout = resource.dynamicCast<LayoutResource>())
    {
        if (isVideoWallReviewLayout(layout))
            iconType = ResourceIconCache::VideoWall;
        else if (layout::isEncrypted(layout))
            iconType = ResourceIconCache::ExportedEncryptedLayout;
    }

    if (iconType == ResourceIconCache::Server && iconStatus == ResourceIconCache::Offline)
    {
        if (!isCompatibleServer(resource) && !isDetachedServer(resource))
            iconStatus = ResourceIconCache::Incompatible;
    }

    return ResourceIconCache::Key(iconType | iconStatus | iconFlags);
}

} // namespace nx::vms::client::desktop
