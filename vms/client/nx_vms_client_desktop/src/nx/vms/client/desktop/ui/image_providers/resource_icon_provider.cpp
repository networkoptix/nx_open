// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_provider.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_model_adapter.h>

namespace nx::vms::client::desktop {

namespace {

QIcon::Mode iconMode(ResourceTree::ItemState itemState)
{
    switch (itemState)
    {
        case ResourceTree::ItemState::normal:
            return QIcon::Normal;

        case ResourceTree::ItemState::selected:
            return QIcon::Selected;

        case ResourceTree::ItemState::accented:
            return QIcon::Active;

        default:
            NX_ASSERT(false);
            return QIcon::Normal;
    }
}

} // namespace

QPixmap ResourceIconProvider::requestPixmap(
    const QString& id, QSize* /*size*/, const QSize& /*requestedSize*/)
{
    auto path = id.split("/", Qt::SkipEmptyParts);
    const int key = path.empty() ? 0 : path.takeFirst().toInt();
    if (key == 0)
        return {};

    const auto state = path.empty()
        ? ResourceTree::ItemState::normal
        : ResourceTree::ItemState(path.takeFirst().toInt());

    return Skin::maximumSizePixmap(
        qnResIconCache->icon(QnResourceIconCache::Key(key)),
        iconMode(state));
}

} // namespace nx::vms::client::desktop
