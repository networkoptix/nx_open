// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "functional_delegate_utilities.h"

#include <client/client_globals.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>

namespace nx::vms::client::desktop {

QVariant resourceVersionAccessor(const QnResourcePtr& ptr, int role)
{
    auto server = ptr.dynamicCast<QnMediaServerResource>();
    if (server)
    {
        switch (role)
        {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
            case Qt::StatusTipRole:
            case Qt::WhatsThisRole:
            case Qt::AccessibleTextRole:
            case Qt::AccessibleDescriptionRole:
                return server->getVersion().toString();
            case core::MediaServerResourceRole:
                return QVariant::fromValue<QnMediaServerResourcePtr>(server);
        }
    }
    return QVariant();
}

CustomizableItemDelegate* makeVersionStatusDelegate(QnWorkbenchContext* context, QObject* parent)
{
    auto watcher = context->instance<QnWorkbenchVersionMismatchWatcher>();
    const auto latestMsVersion = watcher->latestVersion(
        QnWorkbenchVersionMismatchWatcher::Component::server);

    auto delegate = new CustomizableItemDelegate(parent);
    delegate->setCustomInitStyleOption(
        [latestMsVersion](QStyleOptionViewItem* item, const QModelIndex& index)
        {
            auto ptr = index.data(core::MediaServerResourceRole).value<QnMediaServerResourcePtr>();
            if (!ptr)
                return;

            const bool updateRequested = QnWorkbenchVersionMismatchWatcher::versionMismatches(
                ptr->getVersion(), latestMsVersion, true);
            if (updateRequested)
            {
                item->palette.setColor(QPalette::Text, core::colorTheme()->color("red_l2"));
            }
        });
    return delegate;
}

} // namespace nx::vms::client::desktop
