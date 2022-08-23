// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_dialog.h"
#include "ui_failover_priority_dialog.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_dialogs/failover_priority_view_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

namespace nx::vms::client::desktop {

FailoverPriorityDialog::FailoverPriorityDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::FailoverPriorityDialog())
{
    ui->setupUi(this);

    const auto failoverPriorityViewWidget = new FailoverPriorityViewWidget(this);
    ui->widgetLayout->insertWidget(0, failoverPriorityViewWidget);

    setHelpTopic(this, Qn::ServerSettings_Failover_Help);

    m_applyFailoverPriority =
        [this, failoverPriorityViewWidget]
        {
            const auto modifiedFailoverPriority =
                failoverPriorityViewWidget->modifiedFailoverPriority();

            if (modifiedFailoverPriority.isEmpty())
                return;

            qnResourcesChangesManager->saveCameras(modifiedFailoverPriority.keys(),
                [&modifiedFailoverPriority](const QnVirtualCameraResourcePtr& camera)
                {
                    camera->setFailoverPriority(modifiedFailoverPriority[camera]);
                });
        };
}

FailoverPriorityDialog::~FailoverPriorityDialog()
{
}

void FailoverPriorityDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    if (button != QDialogButtonBox::Ok)
        return;

    m_applyFailoverPriority();
}

} // namespace nx::vms::client::desktop
