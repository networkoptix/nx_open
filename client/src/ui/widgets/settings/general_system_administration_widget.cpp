#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(QWidget *parent /*= NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralSystemAdministrationWidget)
{
    ui->setupUi(this);

    connect(ui->businessRulesButton,    &QPushButton::clicked, this, [this] { menu()->trigger(Qn::OpenBusinessRulesAction); } );
    connect(ui->cameraListButton,       &QPushButton::clicked, this, [this] { menu()->trigger(Qn::CameraListAction); } );
    connect(ui->eventLogButton,         &QPushButton::clicked, this, [this] { menu()->trigger(Qn::BusinessEventsLogAction); } );
    connect(ui->healthMonitorButton,    &QPushButton::clicked, this, [this] { menu()->trigger(Qn::OpenInNewLayoutAction, qnResPool->getResourcesWithFlag(Qn::server)); } );
}

void QnGeneralSystemAdministrationWidget::updateFromSettings() {
    ui->cameraWidget->updateFromSettings();
    ui->backupWidget->updateFromSettings();
}

void QnGeneralSystemAdministrationWidget::submitToSettings() {
    ui->cameraWidget->submitToSettings();
    ui->backupWidget->submitToSettings();
}

bool QnGeneralSystemAdministrationWidget::hasChanges() const  {
    return ui->cameraWidget->hasChanges();
}
