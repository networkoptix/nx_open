#include "functional_delegate_utilities.h"

#include <core/resource/media_server_resource.h>
#include <ui/delegates/customizable_item_delegate.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>

namespace nx {
namespace client {
namespace desktop {

QVariant resourceVersionAccessor(QnResourcePtr ptr, int role)
{
    QnMediaServerResourcePtr server = ptr.dynamicCast<QnMediaServerResource>();
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
        case Qn::MediaServerResourceRole:
            return QVariant::fromValue<QnMediaServerResourcePtr>(server);
        }
    }
    return QVariant();
}

QnCustomizableItemDelegate * makeVersionStatusDelegate(QnWorkbenchContext* context, QObject* parent)
{
    QnWorkbenchVersionMismatchWatcher *watcher = context->instance<QnWorkbenchVersionMismatchWatcher>();
    QnSoftwareVersion latestMsVersion = watcher->latestVersion(Qn::ServerComponent);

    auto* delegate = new QnCustomizableItemDelegate(parent);
    delegate->setCustomInitStyleOption(
        [latestMsVersion](QStyleOptionViewItem* item, const QModelIndex& index)
        {
            QnMediaServerResourcePtr ptr = index.data(Qn::MediaServerResourceRole).value<QnMediaServerResourcePtr>();
            if (!ptr)
                return;

            bool updateRequested = QnWorkbenchVersionMismatchWatcher::versionMismatches(ptr->getVersion(), latestMsVersion, true);
            if (updateRequested)
            {
                QColor color = qnGlobals->errorTextColor();
                item->palette.setColor(QPalette::Text, color);
            }
        });
    return delegate;
}

} // namespace desktop
} // namespace client
} // namespace nx
