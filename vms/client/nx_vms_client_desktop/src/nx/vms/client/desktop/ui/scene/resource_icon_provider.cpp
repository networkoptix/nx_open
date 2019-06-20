#include "resource_icon_provider.h"

#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

#include <nx/vms/client/desktop/ui/scene/models/resource_tree_model_adapter.h>

namespace nx::vms::client::desktop {

namespace {

QIcon::Mode iconMode(ResourceTreeModelAdapter::ItemState itemState)
{
    switch (itemState)
    {
        case ResourceTreeModelAdapter::ItemState::normal:
            return QIcon::Normal;

        case ResourceTreeModelAdapter::ItemState::selected:
            return QIcon::Selected;

        case ResourceTreeModelAdapter::ItemState::accented:
            return QIcon::Active;

        default:
            NX_ASSERT(false);
            return QIcon::Normal;
    }
}

} // namespace

QPixmap ResourceIconProvider::requestPixmap(
    const QString& id, QSize* size, const QSize& requestedSize)
{
    auto path = id.split("/", QString::SplitBehavior::SkipEmptyParts);
    const int key = path.empty() ? 0 : path.takeFirst().toInt();
    if (key == 0)
        return {};

    using ItemState = ResourceTreeModelAdapter::ItemState;
    const auto state = path.empty() ? ItemState::normal: ItemState(path.takeFirst().toInt());

    return QnSkin::maximumSizePixmap(
        qnResIconCache->icon(QnResourceIconCache::Key(key)),
        iconMode(state));
}

} // namespace nx::vms::client::desktop
