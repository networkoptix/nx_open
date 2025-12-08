// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/skin/resource_icon_cache.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ResourceIconCache: public core::ResourceIconCache
{
    using base_type = core::ResourceIconCache;
public:
    ResourceIconCache(QObject* parent = nullptr);
    virtual ~ResourceIconCache();

    virtual Key key(const QnResourcePtr& resource) const override;
    virtual QString iconPath(Key key) const override;

private:
    QString iconPathFromLegacyIcons(Key key) const;
    QHash<Key, QString> m_legacyIconsPaths;
};

} // namespace nx::vms::client::desktop
