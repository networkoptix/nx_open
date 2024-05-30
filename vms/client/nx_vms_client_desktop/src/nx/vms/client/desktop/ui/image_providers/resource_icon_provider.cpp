// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_icon_provider.h"

#include <QtCore/QUrlQuery>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
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
    const QString& idAndParams, QSize* /*size*/, const QSize& requestedSize)
{
    const auto url = QUrl(idAndParams);
    const auto urlQuery = QUrlQuery(url.query());
    const auto queryItems = urlQuery.queryItems();

    auto path = url.path().split("/", Qt::SkipEmptyParts);
    const int keyInt = path.empty() ? 0 : path.takeFirst().toInt();
    if (!NX_ASSERT(keyInt, "Incorrect id"))
        return {};

    const auto mode = iconMode(path.empty() ? ResourceTree::ItemState::normal
                                            : ResourceTree::ItemState(path.takeFirst().toInt()));

    core::SvgIconColorer::ThemeSubstitutions overriddenColors;
    for (const auto& queryValue: queryItems)
        overriddenColors[mode][queryValue.first] = queryValue.second;

    return qnResIconCache->iconPixmap(
        QnResourceIconCache::Key(keyInt), requestedSize, overriddenColors, mode);
}

} // namespace nx::vms::client::desktop
