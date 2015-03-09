#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

#include <api/runtime_info_manager.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_runtime_data.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(QWidget *parent /*= NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralSystemAdministrationWidget)
{
    ui->setupUi(this);

    auto shortcutString = [this](const Qn::ActionId actionId, const QString &baseString) -> QString {
        auto shortcut = action(actionId)->shortcut();
        if (shortcut.isEmpty())
            return baseString;
        return lit("%1 (<b>%2</b>)")
            .arg(baseString)
            .arg(shortcut.toString(QKeySequence::NativeText));
    };

    ui->eventRulesLabel->setText(shortcutString(Qn::BusinessEventsAction, tr("Open Alarm/Event Rules Management")));
    ui->eventLogLabel->setText(shortcutString(Qn::BusinessEventsLogAction, tr("Open Event Log")));
    ui->cameraListLabel->setText(shortcutString(Qn::CameraListAction, tr("Open Camera List")));

    setHelpTopic(ui->businessRulesButton,   Qn::EventsActions_Help);
    setHelpTopic(ui->cameraListButton,      Qn::Administration_General_CamerasList_Help);
    setHelpTopic(ui->eventLogButton,        Qn::EventLog_Help);
    setHelpTopic(ui->healthMonitorButton,   Qn::Administration_General_HealthMonitoring_Help);

    connect(ui->businessRulesButton,    &QPushButton::clicked,  this, [this] { menu()->trigger(Qn::OpenBusinessRulesAction); } );
    connect(ui->cameraListButton,       &QPushButton::clicked, this, [this] { menu()->trigger(Qn::CameraListAction); } );
    connect(ui->eventLogButton,         &QPushButton::clicked, this, [this] { menu()->trigger(Qn::BusinessEventsLogAction); } );
    connect(ui->healthMonitorButton,    &QPushButton::clicked, this, [this] { menu()->trigger(Qn::OpenInNewLayoutAction, qnResPool->getResourcesWithFlag(Qn::server)); } );
}

void QnGeneralSystemAdministrationWidget::updateFromSettings() {
    ui->cameraWidget->updateFromSettings();
    ui->backupWidget->updateFromSettings();
    ui->backupGroupBox->setVisible(isDatabaseBackupAvailable());
}

void QnGeneralSystemAdministrationWidget::submitToSettings() {
    ui->cameraWidget->submitToSettings();
    ui->backupWidget->submitToSettings();
}

bool QnGeneralSystemAdministrationWidget::hasChanges() const  {
    return ui->cameraWidget->hasChanges();
}

void QnGeneralSystemAdministrationWidget::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);

    QList<QPushButton*> buttons = QList<QPushButton*>()
        << ui->businessRulesButton
        << ui->cameraListButton
        << ui->eventLogButton
        << ui->healthMonitorButton;

    int maxWidht = (*std::max_element(buttons.cbegin(), buttons.cend(), [](QPushButton* l, QPushButton* r){
        return l->width() < r->width();
    }))->width();
    for(QPushButton* button: buttons)
        button->setMinimumWidth(maxWidht);

    updateGeometry();
}

bool QnGeneralSystemAdministrationWidget::isDatabaseBackupAvailable() const {
    return QnRuntimeInfoManager::instance()->remoteInfo().data.box != lit("isd");
}
