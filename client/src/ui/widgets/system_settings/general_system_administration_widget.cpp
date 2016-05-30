#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

#include <api/runtime_info_manager.h>

#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_runtime_data.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(QWidget *parent /* = NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralSystemAdministrationWidget)
{
    ui->setupUi(this);

    retranslateUi();

    setHelpTopic(ui->businessRulesButton,   Qn::EventsActions_Help);
    setHelpTopic(ui->cameraListButton,      Qn::Administration_General_CamerasList_Help);
    setHelpTopic(ui->auditLogButton,        Qn::AuditTrail_Help);
    setHelpTopic(ui->eventLogButton,        Qn::EventLog_Help);
    setHelpTopic(ui->healthMonitorButton,   Qn::Administration_General_HealthMonitoring_Help);
    setHelpTopic(ui->bookmarksButton,       Qn::Bookmarks_Usage_Help);

    connect(ui->businessRulesButton,    &QPushButton::clicked,  this, [this] { menu()->trigger(QnActions::OpenBusinessRulesAction); } );
    connect(ui->cameraListButton,       &QPushButton::clicked, this, [this] { menu()->trigger(QnActions::CameraListAction); } );
    connect(ui->auditLogButton,         &QPushButton::clicked, this, [this] { menu()->trigger(QnActions::OpenAuditLogAction); } );
    connect(ui->eventLogButton,         &QPushButton::clicked, this, [this] { menu()->trigger(QnActions::OpenBusinessLogAction); } );
    connect(ui->healthMonitorButton,    &QPushButton::clicked, this, [this] { menu()->trigger(QnActions::OpenInNewLayoutAction, qnResPool->getResourcesWithFlag(Qn::server)); } );
    connect(ui->bookmarksButton,      &QPushButton::clicked, this, [this] { menu()->trigger(QnActions::OpenBookmarksSearchAction); });

    connect(ui->systemSettingsWidget, &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

void QnGeneralSystemAdministrationWidget::loadDataToUi() {
    ui->systemSettingsWidget->loadDataToUi();
    ui->backupGroupBox->setVisible(isDatabaseBackupAvailable());
}

void QnGeneralSystemAdministrationWidget::applyChanges() {
    ui->systemSettingsWidget->applyChanges();
}

bool QnGeneralSystemAdministrationWidget::hasChanges() const  {
    return ui->systemSettingsWidget->hasChanges();
}

void QnGeneralSystemAdministrationWidget::retranslateUi() {
    auto shortcutString = [this](const QnActions::IDType actionId, const QString &baseString) -> QString {
        auto shortcut = action(actionId)->shortcut();
        if (shortcut.isEmpty())
            return baseString;
        return lit("%1 (<b>%2</b>)")
            .arg(baseString)
            .arg(shortcut.toString(QKeySequence::NativeText));
    };

    ui->eventRulesLabel->setText(shortcutString(QnActions::BusinessEventsAction, tr("Open Alarm/Event Rules Management")));
    ui->eventLogLabel->setText(shortcutString(QnActions::OpenBusinessLogAction, tr("Open Event Log")));
    ui->bookmarksLabel->setText(shortcutString(QnActions::OpenBookmarksSearchAction, tr("Open Bookmarks List")));
    ui->cameraListLabel->setText(shortcutString(QnActions::CameraListAction, QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Open Devices List"),
        tr("Open Cameras List")
        )));


    ui->cameraListButton->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Devices List..."),
        tr("Cameras List...")
        ));

    ui->systemSettingsWidget->retranslateUi();
}


void QnGeneralSystemAdministrationWidget::resizeEvent(QResizeEvent *event) {
    base_type::resizeEvent(event);

    QList<QPushButton*> buttons = QList<QPushButton*>()
        << ui->businessRulesButton
        << ui->cameraListButton
        << ui->auditLogButton
        << ui->eventLogButton
        << ui->bookmarksButton
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

void QnGeneralSystemAdministrationWidget::setReadOnlyInternal(bool readOnly) {
    using ::setReadOnly;

    setReadOnly(ui->systemSettingsWidget, readOnly);
    setReadOnly(ui->backupWidget, readOnly);
}
