#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <limits>

#include <QtWidgets/QMessageBox>

//TODO: #GDM ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource_management/resource_pool.h>
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
    m_inUpdateMaxFps(false),
    m_loginWasEmpty(true),
    m_passwordWasEmpty(true)
{
    ui->setupUi(this);

    ui->cameraScheduleWidget->setContext(context());

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(clicked()),                      this,   SLOT(at_enableAudioCheckBox_clicked()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(at_cameraScheduleWidget_gridParamsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(recordingSettingsChanged()),     this,   SLOT(at_cameraScheduleWidget_recordingSettingsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(controlsChangesApplied()),       this,   SLOT(at_cameraScheduleWidget_controlsChangesApplied()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged()));

    connect(ui->expertSettingsWidget, SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));

    connect(ui->arOverrideCheckBox, &QCheckBox::stateChanged, this, [this](int state){ ui->arComboBox->setEnabled(state == Qt::Checked);} );
    connect(ui->arOverrideCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dbDataChanged()));

    ui->arComboBox->addItem(tr("4:3"),  4.0 / 3);
    ui->arComboBox->addItem(tr("16:9"), 16.0 / 9);
    ui->arComboBox->addItem(tr("1:1"),  1.0);
    ui->arComboBox->setCurrentIndex(0);
    connect(ui->arComboBox,         SIGNAL(currentIndexChanged(int)),    this,          SLOT(at_dbDataChanged()));


    /* Set up context help. */
    setHelpTopic(this,                  Qn::CameraSettings_Multi_Help);
    setHelpTopic(ui->tabRecording,      Qn::CameraSettings_Recording_Help);


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
    } else if(tab == ui->expertTab) {
        return Qn::ExpertCameraSettingsTab;
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
    case Qn::AdvancedCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabRecording);
        break;
    case Qn::ExpertCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->expertTab);
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

    bool overrideAr = ui->arOverrideCheckBox->checkState() == Qt::Checked;
    bool clearAr = ui->arOverrideCheckBox->checkState() == Qt::Unchecked;

    foreach(QnVirtualCameraResourcePtr camera, m_cameras) {
        QString cameraLogin = camera->getAuth().user();
        if (!login.isEmpty() || !m_loginWasEmpty)
            cameraLogin = login;

        QString cameraPassword = camera->getAuth().password();
        if (!password.isEmpty() || !m_passwordWasEmpty)
            cameraPassword = password;

        camera->setAuth(cameraLogin, cameraPassword);

        if (ui->enableAudioCheckBox->checkState() != Qt::PartiallyChecked && ui->enableAudioCheckBox->isEnabled()) 
            camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());

        if (m_hasScheduleEnabledChanges)
            camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());

        if (m_hasScheduleChanges)
            camera->setScheduleTasks(scheduleTasks);

        if (overrideAr)
            camera->setProperty(Qn::customAspectRatioKey, QString::number(ui->arComboBox->itemData(ui->arComboBox->currentIndex()).toDouble()));
        else if (clearAr)
            camera->setProperty(Qn::customAspectRatioKey, QString());
    }
    ui->expertSettingsWidget->submitToResources(m_cameras);

    setHasDbChanges(false);
}

void QnMultipleCameraSettingsWidget::reject()
{
    updateFromResources();
}

bool QnMultipleCameraSettingsWidget::licensedParametersModified() const
{
    return m_hasScheduleEnabledChanges || m_hasScheduleChanges;
}

void QnMultipleCameraSettingsWidget::updateFromResources() {
    if(m_cameras.empty()) {
        ui->loginEdit->setText(QString());
        ui->enableAudioCheckBox->setCheckState(Qt::Unchecked);
        ui->loginEdit->setPlaceholderText(QString());
        ui->passwordEdit->setText(QString());
        ui->passwordEdit->setPlaceholderText(QString());
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::PartiallyChecked);
        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setChangesDisabled(true);
        ui->cameraScheduleWidget->setMotionAvailable(false);
    } else {
        /* Aggregate camera parameters first. */

        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());

        QSet<QString> logins, passwords;
        
        ui->enableAudioCheckBox->setEnabled(true);
    
        ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, true);
        ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, true);

        bool firstCamera = true;

        bool isFirstAr   = true;
        bool sameArOverride = true;
        QString arOverride;

        foreach (const QnVirtualCameraResourcePtr &camera, m_cameras) 
        {
            logins.insert(camera->getAuth().user());
            passwords.insert(camera->getAuth().password());

            if (!camera->isAudioSupported())
                ui->enableAudioCheckBox->setEnabled(false);

            if (camera->isDtsBased()) {
                ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, false);
                ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, false);
            }

            Qt::CheckState audioState = camera->isAudioEnabled() ? Qt::Checked : Qt::Unchecked;
            if (firstCamera) {
                ui->enableAudioCheckBox->setCheckState(audioState);
            } else if (audioState != ui->enableAudioCheckBox->checkState()) {
                ui->enableAudioCheckBox->setCheckState(Qt::PartiallyChecked);
            }

            QString changedAr = camera->getProperty(Qn::customAspectRatioKey);
            if (firstCamera) {
                arOverride = changedAr;
            } else {
                sameArOverride &= changedAr == arOverride;
            }

            firstCamera = false;
        }

        ui->arOverrideCheckBox->setTristate(!sameArOverride);
        if (sameArOverride) {
            ui->arOverrideCheckBox->setChecked(!arOverride.isEmpty());

            // float is important here
            float ar = arOverride.toFloat();
            int idx = -1;
            for (int i = 0; i < ui->arComboBox->count(); ++i) {
                if (qFuzzyEquals(ar, ui->arComboBox->itemData(i).toFloat())) {
                    idx = i;
                    break;
                }
            }
            ui->arComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
        }
        else
            ui->arOverrideCheckBox->setCheckState(Qt::PartiallyChecked);

        ui->expertSettingsWidget->updateFromResources(m_cameras);

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
        m_loginWasEmpty = ui->loginEdit->text().isEmpty();

        if (passwords.size() == 1) {
            ui->passwordEdit->setText(*passwords.begin());
            ui->passwordEdit->setPlaceholderText(QString());
        } else {
            ui->passwordEdit->setText(QString());
            ui->passwordEdit->setPlaceholderText(tr("<multiple values>", "PasswordEdit"));
        }
        m_passwordWasEmpty = ui->passwordEdit->text().isEmpty();

    }

    ui->cameraScheduleWidget->setCameras(m_cameras);

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
    setReadOnly(ui->enableAudioCheckBox, readOnly);
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

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged() {
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
    Qt::CheckState state = ui->enableAudioCheckBox->checkState();

    ui->enableAudioCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->enableAudioCheckBox->setCheckState(Qt::Checked);
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
