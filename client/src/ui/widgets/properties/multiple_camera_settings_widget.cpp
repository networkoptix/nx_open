#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <limits>

#include <QtGui/QMessageBox>

//TODO: #GDM ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <ui/common/read_only.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/widgets/properties/camera_settings_widget_p.h>

#include <utils/license_usage_helper.h>
#include "ui/help/help_topics.h"
#include "ui/help/help_topic_accessor.h"


QnMultipleCameraSettingsWidget::QnMultipleCameraSettingsWidget(QWidget *parent): 
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnCameraSettingsWidgetPrivate()),
    ui(new Ui::MultipleCameraSettingsWidget),
    m_hasDbChanges(false),
    m_hasScheduleChanges(false),
    m_hasScheduleControlsChanges(false),
    m_readOnly(false),
    m_inUpdateMaxFps(false)
{
    ui->setupUi(this);

    ui->cameraScheduleWidget->setContext(context());

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->checkBoxEnableAudio,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->checkBoxEnableAudio,    SIGNAL(clicked()),                      this,   SLOT(at_enableAudioCheckBox_clicked()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(at_cameraScheduleWidget_gridParamsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(recordingSettingsChanged()),     this,   SLOT(at_cameraScheduleWidget_recordingSettingsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(controlsChangesApplied()),       this,   SLOT(at_cameraScheduleWidget_controlsChangesApplied()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged(int)));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(updateLicenseText()), Qt::QueuedConnection);

    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),              this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),              this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(ui->analogViewCheckBox,     SIGNAL(clicked()),                      this,   SLOT(at_analogViewCheckBox_clicked()));
    connect(ui->moreLicensesButton,     SIGNAL(clicked()),                      this,   SIGNAL(moreLicensesRequested()));

    connect(ui->advancedSettingsWidget, SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));

    /* Set up context help. */
    setHelpTopic(ui->tabRecording,                                            Qn::CameraSettings_Recording_Help);
    setHelpTopic(ui->tabAdvancedSettings,                                     Qn::CameraSettings_Advanced_Help);


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
    Q_D(QnCameraSettingsWidget);
    d->setCameras(cameras);

    updateFromResources();
}

Qn::CameraSettingsTab QnMultipleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */
    
    QWidget *tab = ui->tabWidget->currentWidget();

    if(tab == ui->tabRecording) {
        return Qn::RecordingSettingsTab;
    } else if(tab == ui->tabAdvancedSettings) {
        return Qn::AdvancedCameraSettingsTab;
    } else {
        qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
        return Qn::GeneralSettingsTab;
    }
}

void QnMultipleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    switch(tab) {
    case Qn::GeneralSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        break;
    case Qn::RecordingSettingsTab:
    case Qn::MotionSettingsTab:
    case Qn::CameraPropertiesTab:
        ui->tabWidget->setCurrentWidget(ui->tabRecording);
        break;
    case Qn::AdvancedCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabAdvancedSettings);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        break;
    }
}

void QnMultipleCameraSettingsWidget::setScheduleEnabled(bool enabled) {
    ui->cameraScheduleWidget->setScheduleEnabled(enabled);
}

bool QnMultipleCameraSettingsWidget::isScheduleEnabled() const {
    return ui->cameraScheduleWidget->isScheduleEnabled();
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

        // flags can be set if analog and dts-based cameras are selected together
        // and checkBox "Use Analog License" was checked
        if (camera->isAnalog()) {
            if (ui->analogViewCheckBox->checkState() != Qt::PartiallyChecked)
                camera->setScheduleDisabled(ui->analogViewCheckBox->checkState() == Qt::Unchecked);
        } else
        if (m_hasScheduleEnabledChanges)
            camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());

        if (m_hasScheduleChanges)
            camera->setScheduleTasks(scheduleTasks);

        Qn::SecondStreamQuality sQuality = ui->advancedSettingsWidget->secondaryStreamQuality();
        if (sQuality != Qn::SSQualityNotDefined)
            camera->setSecondaryStreamQuality(sQuality);
        Qt::CheckState cs = ui->advancedSettingsWidget->getCameraControl();
        if (cs != Qt::PartiallyChecked)
            camera->setCameraControlDisabled(cs == Qt::Checked);

    }

    setHasDbChanges(false);
}

void QnMultipleCameraSettingsWidget::reject()
{
    updateFromResources();
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
        ui->analogGroupBox->setVisible(false);
    } else {
        /* Aggregate camera parameters first. */

        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());

        QSet<QString> logins, passwords;
        
        ui->checkBoxEnableAudio->setEnabled(true);
    
        ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, true);
        ui->tabWidget->setTabEnabled(Qn::AdvancedCameraSettingsTab, true);
        ui->analogGroupBox->setVisible(true);

        bool firstCamera = true;
        foreach (QnVirtualCameraResourcePtr camera, m_cameras) 
        {
            logins.insert(camera->getAuth().user());
            passwords.insert(camera->getAuth().password());

            if (!camera->isAudioSupported())
                ui->checkBoxEnableAudio->setEnabled(false);

            if (camera->isDtsBased()) {
                ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, false);
        		ui->tabWidget->setTabEnabled(Qn::AdvancedCameraSettingsTab, false);
            }

            if (camera->isAnalog()) {
                Qt::CheckState viewState = camera->isScheduleDisabled() ? Qt::Unchecked : Qt::Checked;
                if (firstCamera)
                    ui->analogViewCheckBox->setCheckState(viewState);
                else if (viewState != ui->analogViewCheckBox->checkState())
                    ui->analogViewCheckBox->setCheckState(Qt::PartiallyChecked);
            } else {
                ui->analogGroupBox->setVisible(false);
                ui->analogViewCheckBox->setCheckState(Qt::PartiallyChecked);
            }

            Qt::CheckState audioState = camera->isAudioEnabled() ? Qt::Checked : Qt::Unchecked;
            if (firstCamera) {
                ui->checkBoxEnableAudio->setCheckState(audioState);
            } else if (audioState != ui->checkBoxEnableAudio->checkState()) {
                ui->checkBoxEnableAudio->setCheckState(Qt::PartiallyChecked);
            }

            firstCamera = false;
        }

        ui->advancedSettingsWidget->updateFromResources(m_cameras);

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
            isMotionAvailable &= camera->getMotionType() != Qn::MT_NoMotion;
        ui->cameraScheduleWidget->setMotionAvailable(isMotionAvailable);

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

    }

    ui->cameraScheduleWidget->setCameras(m_cameras);

    updateLicenseText();

    setHasDbChanges(false);
    m_hasScheduleControlsChanges = false;
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

void QnMultipleCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled){
    ui->cameraScheduleWidget->setExportScheduleButtonEnabled(enabled);
}

void QnMultipleCameraSettingsWidget::setHasDbChanges(bool hasChanges) {
    if(m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;
    if(!m_hasDbChanges) {
        m_hasScheduleChanges = false;
        m_hasScheduleEnabledChanges = false;
    }

    emit hasChangesChanged();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnMultipleCameraSettingsWidget::at_dbDataChanged() {
    setHasDbChanges(true);
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dbDataChanged();

    m_hasScheduleChanges = true;
    m_hasScheduleControlsChanges = false;
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_recordingSettingsChanged() {
    at_dbDataChanged();

    m_hasScheduleChanges = true;
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged(int state) {
    if (state == Qt::PartiallyChecked) {
        ui->analogViewCheckBox->setTristate(true);
        ui->analogViewCheckBox->setCheckState(Qt::PartiallyChecked);
    } else {
        ui->analogViewCheckBox->setTristate(false);
        ui->analogViewCheckBox->setChecked(state == Qt::Checked);
    }
    updateLicenseText();
    at_dbDataChanged();

    m_hasScheduleEnabledChanges = true;
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_gridParamsChanged() {
    m_hasScheduleControlsChanges = true;
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_controlsChangesApplied() {
    m_hasScheduleControlsChanges = false;
}

void QnMultipleCameraSettingsWidget::at_enableAudioCheckBox_clicked() {
    Qt::CheckState state = ui->checkBoxEnableAudio->checkState();

    ui->checkBoxEnableAudio->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->checkBoxEnableAudio->setCheckState(Qt::Checked);
}

void QnMultipleCameraSettingsWidget::at_analogViewCheckBox_clicked() {
    Qt::CheckState state = ui->analogViewCheckBox->checkState();

    ui->analogViewCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->analogViewCheckBox->setCheckState(Qt::Checked);
    ui->cameraScheduleWidget->setScheduleEnabled(ui->analogViewCheckBox->isChecked());
}

void QnMultipleCameraSettingsWidget::updateMaxFPS(){
    if(m_cameras.empty())
        return;

    if (m_inUpdateMaxFps)
        return; /* Do not show message twice. */

    m_inUpdateMaxFps = true;

    int maxFps = std::numeric_limits<int>::max();
    int maxDualStreamingFps = maxFps;

    Q_D(QnCameraSettingsWidget);
    d->calculateMaxFps(&maxFps, &maxDualStreamingFps);

    ui->cameraScheduleWidget->setMaxFps(maxFps, maxDualStreamingFps);
    m_inUpdateMaxFps = false;
}

void QnMultipleCameraSettingsWidget::updateLicenseText() {
    QnLicenseUsageHelper helper;
    if (ui->analogViewCheckBox->checkState() != Qt::PartiallyChecked)
        helper.propose(m_cameras, ui->analogViewCheckBox->checkState() == Qt::Checked);

    //TODO: #GDM refactor duplicated code
    { // digital licenses
        QString usageText = tr("%n digital license(s) are used out of %1.", "", helper.usedDigital()).arg(helper.totalDigital());
        ui->digitalLicensesLabel->setText(usageText);
        QPalette palette = this->palette();
        if (!helper.isValid() && helper.requiredDigital() > 0)
            setWarningStyle(&palette);
        ui->digitalLicensesLabel->setPalette(palette);
    }

    { // analog licenses
        QString usageText = tr("%n analog license(s) are used out of %1.", "", helper.usedAnalog()).arg(helper.totalAnalog());
        ui->analogLicensesLabel->setText(usageText);
        QPalette palette = this->palette();
        if (!helper.isValid() && helper.requiredAnalog() > 0)
            setWarningStyle(&palette);
        ui->analogLicensesLabel->setPalette(palette);
    }

}
