#include "workbench_layout_change_validator.h"

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <ui/dialogs/messages/layouts_handler_messages.h>

QnWorkbenchLayoutsChangeValidator::QnWorkbenchLayoutsChangeValidator(QnWorkbenchContext* context):
    QnWorkbenchContextAware(context)
{
}

bool QnWorkbenchLayoutsChangeValidator::confirmChangeVideoWallLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& removedResources) const
{
    //just in case
    if (!context()->user())
        return true;

    //quick check
    if (accessController()->hasGlobalPermission(Qn::GlobalAccessAllMediaPermission))
        return true;

    QnResourceList inaccessible = removedResources.filtered(
        [this, layout](const QnResourcePtr& resource) -> bool
        {
            QnResourceList providers;
            const auto accessSource = qnResourceAccessProvider->accessibleVia(
                context()->user(), resource, &providers);

            // We need to get only resources which are accessible only by this layout
            if (accessSource != QnAbstractResourceAccessProvider::Source::videowall)
                return false;

            QnLayoutResourceList layoutProviders = providers.filtered<QnLayoutResource>();
            layoutProviders.removeOne(layout);
            return !layoutProviders.isEmpty();
        });

    if (inaccessible.isEmpty())
        return true;

    return QnLayoutsHandlerMessages::changeVideoWallLayout(mainWindow(), inaccessible);
}

