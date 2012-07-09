#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <limits>

#include <QtGui/QMessageBox>

#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "ui/common/read_only.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motion_mask_widget.h"
#include "ui/graphics/items/resource_widget.h"

QnMultipleCameraSettingsWidget::QnMultipleCameraSettingsWidget(QWidget *parent): 
    QWidget(parent),
    ui(new Ui::MultipleCameraSettingsWidget),
    m_hasChanges(false),
    m_hasScheduleChanges(false),
    m_readOnly(false),
    m_inUpdateMaxFps(false)
{
    ui->setupUi(this);

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->checkBoxEnableAudio,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dataChanged()));
    connect(ui->checkBoxEnableAudio,    SIGNAL(clicked()),                  this,       SLOT(at_enableAudioCheckBox_clicked()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged()),       this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));

    updateFromResources();
}

QnMultipleCameraSettingsWidget::~QnMultipleCameraSettingsWidget() {
    return;
}

const QnVirtualCameraResourceList &QnMultipleCameraSettingsWidget::cameras() const {
    return m_cameras;
}

void QnMultipleCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if(m_cameras == cameras)
        return;

    m_cameras = cameras;

    updateFromResources();
}

Qn::CameraSettingsTab QnMultipleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */
    
    QWidget *tab = ui->tabWidget->currentWidget();

    if(tab == ui->tabNetwork) {
        return Qn::NetworkSettingsTab;
    } else if(tab == ui->tabRecording) {
        return Qn::RecordingSettingsTab;
    } else {
        qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
        return Qn::NetworkSettingsTab;
    }
}

void QnMultipleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    switch(tab) {
    case Qn::GeneralSettingsTab:
    case Qn::NetworkSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabNetwork);
        break;
    case Qn::RecordingSettingsTab:
    case Qn::MotionSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabRecording);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        break;
    }
}

int QnMultipleCameraSettingsWidget::activeCameraCount() const {
    return ui->cameraScheduleWidget->activeCameraCount();
}

void QnMultipleCameraSettingsWidget::setCamerasActive(bool active) {
    ui->cameraScheduleWidget->setScheduleEnabled(active);
}

void QnMultipleCameraSettingsWidget::submitToResources() {
    QString login = ui->loginEdit->text().trimmed();
    QString password = ui->passwordEdit->text().trimmed();

    QnScheduleTaskList scheduleTasks;
    if(m_hasScheduleChanges)
        foreach(const QnScheduleTask::Data &data, ui->cameraScheduleWidget->scheduleTasks())
            scheduleTasks.append(QnScheduleTask(data));

    foreach(QnVirtualCameraResourcePtr camera, m_cameras) {
        QString cameraLogin = camera->getAuth().user();
        if (!login.isEmpty())
            cameraLogin = login;

        QString cameraPassword = camera->getAuth().password();
        if (!password.isEmpty())
            cameraPassword = password;

        camera->setAuth(cameraLogin, cameraPassword);

        if (ui->checkBoxEnableAudio->checkState() != Qt::PartiallyChecked && ui->checkBoxEnableAudio->isEnabled()) 
            camera->setAudioEnabled(ui->checkBoxEnableAudio->isChecked());

        if (m_hasScheduleEnabledChanges)
            camera->setScheduleDisabled(ui->cameraScheduleWidget->activeCameraCount() == 0);

        if (m_hasScheduleChanges)
            camera->setScheduleTasks(scheduleTasks);
    }

    setHasChanges(false);
}

void QnMultipleCameraSettingsWidget::updateFromResources() {
    if(m_cameras.empty()) {
        ui->loginEdit->setText(QString());
        ui->checkBoxEnableAudio->setCheckState(Qt::Unchecked);
        ui->loginEdit->setPlaceholderText(QString());
        ui->passwordEdit->setText(QString());
        ui->passwordEdit->setPlaceholderText(QString());
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::PartiallyChecked);
        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setChangesDisabled(true);
        ui->cameraScheduleWidget->setMotionAvailable(false);
    } else {
        /* Aggregate camera parameters first. */

        QSet<QString> logins, passwords;
        
        ui->checkBoxEnableAudio->setEnabled(true);
    
        bool firstCamera = true;
        foreach (QnVirtualCameraResourcePtr camera, m_cameras) 
        {
            logins.insert(camera->getAuth().user());
            passwords.insert(camera->getAuth().password());

            if (!camera->isAudioSupported())
                ui->checkBoxEnableAudio->setEnabled(false);

            Qt::CheckState audioState = camera->isAudioEnabled() ? Qt::Checked : Qt::Unchecked;
            if (firstCamera)
                ui->checkBoxEnableAudio->setCheckState(audioState);
            else if (audioState != ui->checkBoxEnableAudio->checkState())
                ui->checkBoxEnableAudio->setCheckState(Qt::PartiallyChecked);

            firstCamera = false;
        }

        bool isScheduleEqual = true;
        QList<QnScheduleTask::Data> scheduleTasksData;
        foreach (const QnScheduleTask& scheduleTask, m_cameras.front()->getScheduleTasks())
            scheduleTasksData << scheduleTask.getData();

        foreach (QnVirtualCameraResourcePtr camera, m_cameras) {
            QList<QnScheduleTask::Data> cameraScheduleTasksData;
            foreach (const QnScheduleTask& scheduleTask, camera->getScheduleTasks())
                cameraScheduleTasksData << scheduleTask.getData();
            if (cameraScheduleTasksData != scheduleTasksData) {
                isScheduleEqual = false;
                break;
            }
        }
        ui->cameraScheduleWidget->setChangesDisabled(!isScheduleEqual);
        if(isScheduleEqual) {
            ui->cameraScheduleWidget->setScheduleTasks(m_cameras.front()->getScheduleTasks());
        } else {
            ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        }

        updateMaxFPS();

        bool isMotionAvailable = true;
        foreach (QnVirtualCameraResourcePtr camera, m_cameras) 
            isMotionAvailable &= camera->getMotionType() != MT_NoMotion;


        /* Write camera parameters out. */

        if (logins.size() == 1) {
            ui->loginEdit->setText(*logins.begin());
            ui->loginEdit->setPlaceholderText(QString());
        } else {
            ui->loginEdit->setText(QString());
            ui->loginEdit->setPlaceholderText(tr("<multiple values>", "LoginEdit"));
        }

        if (passwords.size() == 1) {
            ui->passwordEdit->setText(*passwords.begin());
            ui->passwordEdit->setPlaceholderText(QString());
        } else {
            ui->passwordEdit->setText(QString());
            ui->passwordEdit->setPlaceholderText(tr("<multiple values>", "PasswordEdit"));
        }

        ui->cameraScheduleWidget->setMotionAvailable(isMotionAvailable);
    }

    ui->cameraScheduleWidget->setCameras(m_cameras);

    setHasChanges(false);
}

bool QnMultipleCameraSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnMultipleCameraSettingsWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->checkBoxEnableAudio, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    m_readOnly = readOnly;
}

void QnMultipleCameraSettingsWidget::setHasChanges(bool hasChanges) {
    if(m_hasChanges == hasChanges)
        return;

    m_hasChanges = hasChanges;
    if(!m_hasChanges) {
        m_hasScheduleChanges = false;
        m_hasScheduleEnabledChanges = false;
    }

    emit hasChangesChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnMultipleCameraSettingsWidget::at_dataChanged() {
    setHasChanges(true);
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dataChanged();

    m_hasScheduleChanges = true;
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged() {
    at_dataChanged();

    m_hasScheduleEnabledChanges = true;
}

void QnMultipleCameraSettingsWidget::at_enableAudioCheckBox_clicked()
{
    Qt::CheckState state = ui->checkBoxEnableAudio->checkState();

    ui->checkBoxEnableAudio->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->checkBoxEnableAudio->setCheckState(Qt::Checked);
}

void QnMultipleCameraSettingsWidget::updateMaxFPS(){
    if(m_cameras.empty())
        return;

    if (m_inUpdateMaxFps)
        return; /* Do not show message twice. */

    m_inUpdateMaxFps = true;

    int maxFps = std::numeric_limits<int>::max();
    foreach (QnVirtualCameraResourcePtr camera, m_cameras) 
    {
        int cameraFps =camera->getMaxFps();
        if ((camera->supportedMotionType() & MT_SoftwareGrid)
            || ui->cameraScheduleWidget->isSecondaryStreamReserver())
            cameraFps -= 2;
        maxFps = qMin(maxFps, cameraFps);
    }
    float currentMaxFps = ui->cameraScheduleWidget->getGridMaxFps();
    if (currentMaxFps > maxFps)
    {
        QMessageBox::warning(this, tr("Warning. FPS value is too high"), 
            tr("For software motion 2 fps is reserved for secondary stream. Current fps in schedule grid is %1. Fps dropped down to %2").arg(currentMaxFps).arg(maxFps));
    }
    ui->cameraScheduleWidget->setMaxFps(maxFps);

    m_inUpdateMaxFps = false;
}