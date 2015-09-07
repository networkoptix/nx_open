#include "camera_settings_dialog.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <api/app_server_connection.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/user_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_target_provider.h>
#include <ui/actions/action_parameters.h>

#include <ui/dialogs/resource_list_dialog.h>

#include <ui/widgets/properties/camera_settings_widget.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <utils/license_usage_helper.h>
#include "core/resource_management/resource_properties.h"

QnCameraSettingsDialog::QnCameraSettingsDialog(QWidget *parent):
    base_type(parent),
    m_selectionUpdatePending(false),
    m_selectionScope(Qn::SceneScope),
    m_ignoreAccept(false)
{
    m_settingsWidget = new QnCameraSettingsWidget(this);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_applyButton = m_buttonBox->button(QDialogButtonBox::Apply);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    
    m_openButton = new QPushButton(tr("Open in New Tab"));
    m_buttonBox->addButton(m_openButton, QDialogButtonBox::HelpRole);

    m_diagnoseButton = new QPushButton();
    m_buttonBox->addButton(m_diagnoseButton, QDialogButtonBox::HelpRole);

    m_rulesButton = new QPushButton();
    m_buttonBox->addButton(m_rulesButton, QDialogButtonBox::HelpRole);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_settingsWidget);
    layout->addWidget(m_buttonBox);

    //connect(m_buttonBox,        &QDialogButtonBox::accepted,        this,   &QnCameraSettingsDialog::acceptIfSafe);

    connect(m_settingsWidget,   &QnCameraSettingsWidget::hasChangesChanged,         this,   &QnCameraSettingsDialog::at_settingsWidget_hasChangesChanged);
    connect(m_settingsWidget,   &QnCameraSettingsWidget::modeChanged,               this,   &QnCameraSettingsDialog::at_settingsWidget_modeChanged);
    
    connect(m_openButton,       &QPushButton::clicked,              this,   &QnCameraSettingsDialog::at_openButton_clicked);
    connect(m_diagnoseButton,   &QPushButton::clicked,              this,   &QnCameraSettingsDialog::at_diagnoseButton_clicked);
    connect(m_rulesButton,      &QPushButton::clicked,              this,   &QnCameraSettingsDialog::at_rulesButton_clicked);

    connect(m_settingsWidget,   &QnCameraSettingsWidget::scheduleExported, this,    &QnCameraSettingsDialog::saveCameras);
    connect(m_settingsWidget,   &QnCameraSettingsWidget::resourcesChanged, this,   &QnCameraSettingsDialog::updateReadOnly);
    
    connect(context(),          &QnWorkbenchContext::userChanged,          this,   &QnCameraSettingsDialog::updateReadOnly);

    connect(action(Qn::SelectionChangeAction),  &QAction::triggered,    this,   &QnCameraSettingsDialog::at_selectionChangeAction_triggered);

    connect(propertyDictionary, &QnResourcePropertyDictionary::asyncSaveDone, this, &QnCameraSettingsDialog::at_cameras_properties_saved);

    at_settingsWidget_hasChangesChanged();
    retranslateUi();
}

QnCameraSettingsDialog::~QnCameraSettingsDialog() {
}

void QnCameraSettingsDialog::retranslateUi() {
    auto cameras = m_settingsWidget->cameras();

    //: "Cameras settings" or "Devices settings" or "IO Module Settings", etc
    setWindowTitle(tr("%1 Settings").arg(getDefaultDevicesName(cameras)));

    //: "Cameras Diagnostics" or "Devices Diagnostics" or "IO Module Diagnostics", etc
    m_diagnoseButton->setText(tr("%1 Diagnostics").arg(getDefaultDevicesName(cameras)));

    //: "Cameras Rules" or "Devices Rules" or "IO Module Rules", etc
    m_rulesButton->setText(tr("%1 Rules").arg(getDefaultDevicesName(cameras)));
}


bool QnCameraSettingsDialog::tryClose(bool force) {
    setCameras(QnVirtualCameraResourceList(), force);
    if (force)
        hide();
    return true;
}


void QnCameraSettingsDialog::accept() {
    if (m_ignoreAccept) {
        m_ignoreAccept = false;
        return;
    }
    base_type::accept();
}

void QnCameraSettingsDialog::reject() {
    m_settingsWidget->reject();
    base_type::reject();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraSettingsDialog::at_settingsWidget_hasChangesChanged() {
    bool hasChanges = m_settingsWidget->hasDbChanges();
    m_applyButton->setEnabled(hasChanges);
    m_settingsWidget->setExportScheduleButtonEnabled(!hasChanges);
}

void QnCameraSettingsDialog::at_settingsWidget_modeChanged() {
    QnCameraSettingsWidget::Mode mode = m_settingsWidget->mode();
    m_okButton->setEnabled(mode == QnCameraSettingsWidget::SingleMode || mode == QnCameraSettingsWidget::MultiMode);
    m_openButton->setVisible(mode == QnCameraSettingsWidget::SingleMode || mode == QnCameraSettingsWidget::MultiMode);
    m_diagnoseButton->setVisible(mode == QnCameraSettingsWidget::SingleMode || mode == QnCameraSettingsWidget::MultiMode);
    m_rulesButton->setVisible(mode == QnCameraSettingsWidget::SingleMode);  //TODO: #GDM implement
}

void QnCameraSettingsDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button) {
    switch(button) {
    case QDialogButtonBox::Ok:
    case QDialogButtonBox::Apply:
        submitToResources(true);
        break;
    case QDialogButtonBox::Cancel:
        m_settingsWidget->reject();
        break;
    default:
        break;
    }
}

void QnCameraSettingsDialog::at_diagnoseButton_clicked() {
    menu()->trigger(Qn::CameraIssuesAction, m_settingsWidget->cameras());
}

void QnCameraSettingsDialog::at_rulesButton_clicked() {
    menu()->trigger(Qn::CameraBusinessRulesAction, m_settingsWidget->cameras());
}

void QnCameraSettingsDialog::updateReadOnly() {
    Qn::Permissions permissions = accessController()->permissions(m_settingsWidget->cameras());
    m_settingsWidget->setReadOnly(!(permissions & Qn::WritePermission));
}

void QnCameraSettingsDialog::setCameras(const QnVirtualCameraResourceList &cameras, bool force /* = false*/) {

    bool askConfirmation = 
           !force
        &&  isVisible()
        &&  m_settingsWidget->cameras() != cameras
        && !m_settingsWidget->cameras().isEmpty()
        &&  (m_settingsWidget->hasDbChanges());

    if (askConfirmation) {
        auto notSavedCameras = m_settingsWidget->cameras();
        QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
            this,
            notSavedCameras,
            tr("%1 not saved.").arg(getDefaultDevicesName(notSavedCameras)),
            //: "Apply changes to the following 5 cameras?" or "Apply changes to the following IO module?", etc
            tr("Apply changes to the following %1?").arg(getNumericDevicesName(notSavedCameras, false)),
            QDialogButtonBox::Yes | QDialogButtonBox::No
            );
        if(button == QDialogButtonBox::Yes)
            submitToResources();
    }

    m_settingsWidget->setCameras(cameras);
    retranslateUi();
}

void QnCameraSettingsDialog::submitToResources(bool checkControls /* = false*/) {
    bool hasDbChanges = m_settingsWidget->hasDbChanges();

    if (checkControls && m_settingsWidget->hasScheduleControlsChanges()){
        QString message = tr("Recording settings have not been saved. Please choose desired recording method, FPS, and quality - then mark the changes on the schedule.");
        int button = QMessageBox::warning(this, tr("Changes have not been applied."), message, QMessageBox::Retry, QMessageBox::Ignore);
        if (button == QMessageBox::Retry) {
            m_ignoreAccept = true;
            return;
        } else {
            m_settingsWidget->clearScheduleControlsChanges();
        }
    } else if (checkControls && m_settingsWidget->hasMotionControlsChanges()){
        QString message = tr("Motion sensitivity has not changed. To change motion sensitivity draw rectangle on the image.");
        int button = QMessageBox::warning(this, tr("Changes have not been applied."), message, QMessageBox::Retry, QMessageBox::Ignore);
        if (button == QMessageBox::Retry){
            m_ignoreAccept = true;
            return;
        } else {
            m_settingsWidget->clearMotionControlsChanges();
        }
    }

    if (!hasDbChanges) {
        return;
    }

    QnVirtualCameraResourceList cameras = m_settingsWidget->cameras();
    if(cameras.empty())
        return;

    /* Dialog will be shown inside */
    if (!m_settingsWidget->isValidMotionRegion()) {
        m_ignoreAccept = true;
        return;
    }

    /* Dialog will be shown inside */
    if (!m_settingsWidget->isValidSecondStream()) {
        m_ignoreAccept = true;
        return;
    }

    //checking if showing Licenses limit exceeded is appropriate
    if(m_settingsWidget->licensedParametersModified() )
    {
        QnCamLicenseUsageHelper helper(cameras, m_settingsWidget->isScheduleEnabled());
        if (!helper.isValid())
        {
            QString message = tr("License limit exceeded. Changes have been saved, but will not be applied.");
            QMessageBox::warning(this, tr("Could not apply changes."), message);
            m_settingsWidget->setScheduleEnabled(false);
        }
    }

    /* Submit and save it. */
    m_settingsWidget->submitToResources();
    saveCameras(cameras);
}

void QnCameraSettingsDialog::at_cameras_saved(ec2::ErrorCode errorCode, const QnVirtualCameraResourceList& cameras) {
    if( errorCode == ec2::ErrorCode::ok )
        return;

    QnResourceListDialog::exec(this, cameras,
        tr("Error"),
        //: "Could not apply changes for the following 5 cameras." or "Could not apply changes for the following IO Module.", etc
        tr("Could not apply changes for the following %1.").arg(getNumericDevicesName(cameras, false)),
        QDialogButtonBox::Ok);
}

void QnCameraSettingsDialog::at_cameras_properties_saved(int requestId, ec2::ErrorCode errorCode) 
{
    QN_UNUSED(requestId);
    if( errorCode == ec2::ErrorCode::ok )
        return;
}

void QnCameraSettingsDialog::saveCameras(const QnVirtualCameraResourceList &cameras) {
    if (cameras.isEmpty())
        return;
    QList<QnUuid> idList = idListFromResList(cameras);
    for( const QnVirtualCameraResourcePtr& camera: cameras )
        if( camera->preferedServerId().isNull() )
            camera->setPreferedServerId( camera->getParentId() );
    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->saveUserAttributes(
        QnCameraUserAttributePool::instance()->getAttributesList(idList),
        this,
        [this, cameras]( int reqID, ec2::ErrorCode errorCode ) {
            Q_UNUSED(reqID);
            at_cameras_saved(errorCode, cameras); 
    } );
    propertyDictionary->saveParamsAsync(idList);
}

void QnCameraSettingsDialog::at_openButton_clicked() {
    QnVirtualCameraResourceList cameras = m_settingsWidget->cameras();
    menu()->trigger(Qn::OpenInNewLayoutAction, cameras);
    m_settingsWidget->setCameras(cameras);
    retranslateUi();
    m_selectionUpdatePending = false;
}

void QnCameraSettingsDialog::updateCamerasFromSelection() {
    if(!m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = false;

    if (isHidden())
        return;

    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;

    Qn::ActionScope scope = provider->currentScope();
    if(scope != Qn::SceneScope && scope != Qn::TreeScope) {
        scope = m_selectionScope;
    } else {
        m_selectionScope = scope;
    }

    menu()->trigger(Qn::OpenInCameraSettingsDialogAction, provider->currentParameters(scope));
}

void QnCameraSettingsDialog::at_selectionChangeAction_triggered() {
    if(isHidden() || m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = true;
    QTimer::singleShot(50, this, SLOT(updateCamerasFromSelection()));
}

void QnCameraSettingsDialog::at_camera_settings_saved(
        int /*httpStatusCode*/, const QList<QPair<QString, bool> >& /*operationResult*/) {

}

