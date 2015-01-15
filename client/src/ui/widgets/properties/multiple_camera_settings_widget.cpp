#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <limits>

#include <QtWidgets/QMessageBox>

//TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
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

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <utils/aspect_ratio.h>

namespace {

class QnScopedUpdateRollback {
public:
    QnScopedUpdateRollback( QnCameraScheduleWidget* widget ) :
        m_widget(widget) {
            widget->beginUpdate();
    }
    ~QnScopedUpdateRollback() {
        m_widget->endUpdate();
    }
private:
    QnCameraScheduleWidget* m_widget;
};

}// namespace


QnMultipleCameraSettingsWidget::QnMultipleCameraSettingsWidget(QWidget *parent): 
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnCameraSettingsWidgetPrivate()),
    ui(new Ui::MultipleCameraSettingsWidget),
    m_hasDbChanges(false),
    m_hasScheduleChanges(false),
    m_loginWasEmpty(true),
    m_passwordWasEmpty(true),
    m_hasScheduleControlsChanges(false),
    m_readOnly(false),
    m_inUpdateMaxFps(false)
{
    ui->setupUi(this);

    QnCamLicenseUsageHelper helper;
    ui->licensesUsageWidget->init(&helper);

    ui->cameraScheduleWidget->setContext(context());
    connect(context(), &QnWorkbenchContext::userChanged, this, &QnMultipleCameraSettingsWidget::updateLicensesButtonVisible);

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(clicked()),                      this,   SLOT(at_enableAudioCheckBox_clicked()));
    connect(ui->fisheyeCheckBox,        SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->fisheyeCheckBox,        SIGNAL(clicked()),                      this,   SLOT(at_fisheyeCheckBox_clicked()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(at_cameraScheduleWidget_gridParamsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(recordingSettingsChanged()),     this,   SLOT(at_cameraScheduleWidget_recordingSettingsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(controlsChangesApplied()),       this,   SLOT(at_cameraScheduleWidget_controlsChangesApplied()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged(int)));

    connect(ui->moreLicensesButton,     &QPushButton::clicked,                  this,   &QnMultipleCameraSettingsWidget::moreLicensesRequested);
    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),              this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),              this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(ui->analogViewCheckBox,     SIGNAL(clicked()),                      this,   SLOT(at_analogViewCheckBox_clicked()));

    connect(ui->expertSettingsWidget, SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));

    connect(ui->arOverrideCheckBox, &QCheckBox::stateChanged, this, [this](int state){ ui->arComboBox->setEnabled(state == Qt::Checked);} );
    connect(ui->arOverrideCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(archiveRangeChanged()),          this,   SLOT(at_dbDataChanged()));

    ui->arComboBox->addItem(tr("4:3"),  4.0f / 3);
    ui->arComboBox->addItem(tr("16:9"), 16.0f / 9);
    ui->arComboBox->addItem(tr("1:1"),  1.0f);
    ui->arComboBox->setCurrentIndex(0);
    connect(ui->arComboBox,         SIGNAL(currentIndexChanged(int)),    this,          SLOT(at_dbDataChanged()));

    connect(ui->rotCheckBox,&QCheckBox::stateChanged,this,[this](int state){ ui->rotComboBox->setEnabled(state == Qt::Checked);} );
    connect(ui->rotCheckBox,SIGNAL(stateChanged(int)),this,SLOT(at_dbDataChanged()));
    ui->rotComboBox->addItem(tr("0 degrees"),0);
    ui->rotComboBox->addItem(tr("90 degrees"),90);
    ui->rotComboBox->addItem(tr("180 degrees"),180);
    ui->rotComboBox->addItem(tr("270 degrees"),270);
    connect(ui->rotComboBox,SIGNAL(currentIndexChanged(int)),this,SLOT(at_dbDataChanged()));

    /* Set up context help. */
    setHelpTopic(this,                  Qn::CameraSettings_Multi_Help);
    setHelpTopic(ui->tabRecording,      Qn::CameraSettings_Recording_Help);
    setHelpTopic(ui->fisheyeCheckBox,   Qn::CameraSettings_Dewarping_Help);


    updateFromResources();
    updateLicensesButtonVisible();
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
        if (ui->tabWidget->isTabEnabled(Qn::RecordingSettingsTab))
            ui->tabWidget->setCurrentWidget(ui->tabRecording);
        else
            ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        break;
    case Qn::ExpertCameraSettingsTab:
        if (ui->tabWidget->isTabEnabled(Qn::ExpertCameraSettingsTab))
            ui->tabWidget->setCurrentWidget(ui->expertTab);
        else
            ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        ui->tabWidget->setCurrentWidget(ui->tabGeneral);
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
    bool overrideRotation = ui->rotCheckBox->checkState() == Qt::Checked;
    bool clearRotation = ui->rotCheckBox->checkState() == Qt::Unchecked;

    foreach(QnVirtualCameraResourcePtr camera, m_cameras) 
    {
        QString cameraLogin = camera->getAuth().user();
        if (!login.isEmpty() || !m_loginWasEmpty)
            cameraLogin = login;

        QString cameraPassword = camera->getAuth().password();
        if (!password.isEmpty() || !m_passwordWasEmpty)
            cameraPassword = password;

        camera->setAuth(cameraLogin, cameraPassword);

        int maxDays = ui->cameraScheduleWidget->maxRecordedDays();
        if (maxDays != QnCameraScheduleWidget::RecordedDaysDontChange)
            camera->setMaxDays(maxDays);
        int minDays = ui->cameraScheduleWidget->minRecordedDays();
        if (minDays != QnCameraScheduleWidget::RecordedDaysDontChange)
            camera->setMinDays(minDays);
        
        if (ui->enableAudioCheckBox->checkState() != Qt::PartiallyChecked && camera->isAudioSupported()) 
            camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());

        if (ui->fisheyeCheckBox->checkState() != Qt::PartiallyChecked) {
            auto params = camera->getDewarpingParams();
            params.enabled = ui->fisheyeCheckBox->isChecked();
            camera->setDewarpingParams(params);
        }

        if (m_hasScheduleEnabledChanges)
            camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());

        if (m_hasScheduleChanges)
            camera->setScheduleTasks(scheduleTasks);

        if (overrideAr)
            camera->setCustomAspectRatio(ui->arComboBox->currentData().toReal());
        else if (clearAr)
            camera->clearCustomAspectRatio();

        if(overrideRotation) 
            camera->setProperty(QnMediaResource::rotationKey(), QString::number(ui->rotComboBox->currentData().toInt()));
        else if (clearRotation && camera->hasProperty(QnMediaResource::rotationKey()))
            camera->setProperty(QnMediaResource::rotationKey(), QString());

    }
    ui->expertSettingsWidget->submitToResources(m_cameras);

    setHasDbChanges(false);
}

void QnMultipleCameraSettingsWidget::reject()
{
    updateFromResources();
}

bool QnMultipleCameraSettingsWidget::isValidSecondStream() {
    /* Do not check validness if there is no recording anyway. */
    if (!isScheduleEnabled())
        return true;
// 
//     if (!m_camera->hasDualStreaming())
//         return true;

    QList<QnScheduleTask::Data> filteredTasks;
    bool usesSecondStream = false;
    foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks()) {
        QnScheduleTask::Data data(scheduleTaskData);
        if (data.m_recordType == Qn::RT_MotionAndLowQuality) {
            usesSecondStream = true;
            data.m_recordType = Qn::RT_Always;
        }
        filteredTasks.append(data);
    }

    /* There are no Motion+LQ tasks. */
    if (!usesSecondStream)
        return true;

    if (ui->expertSettingsWidget->isSecondStreamEnabled())
        return true;

    auto button = QMessageBox::warning(this,
        tr("Invalid schedule"),
        tr("Second stream is disabled on this camera. Motion + LQ option has no effect."\
        "Press \"Yes\" to change recording type to \"Always\" or \"No\" to re-enable second stream."),
        QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No | QMessageBox::Cancel),
        QMessageBox::Yes);
    switch (button) {
    case QMessageBox::Yes:
        ui->cameraScheduleWidget->setScheduleTasks(filteredTasks);
        return true;
    case QMessageBox::No:
        ui->expertSettingsWidget->setSecondStreamEnabled();
        return true;
    default:
        return false;
    }
}

bool QnMultipleCameraSettingsWidget::licensedParametersModified() const
{
    return m_hasScheduleEnabledChanges || m_hasScheduleChanges;
}

void QnMultipleCameraSettingsWidget::updateFromResources() {
    // This scope will trigger QnScopedUpdateRollback
    {
        QnScopedUpdateRollback rollback(ui->cameraScheduleWidget);
        if(m_cameras.empty()) {
            ui->loginEdit->setText(QString());
            ui->enableAudioCheckBox->setCheckState(Qt::Unchecked);
            ui->fisheyeCheckBox->setCheckState(Qt::Unchecked);
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
            ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, true);
            ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, true);
            ui->analogGroupBox->setVisible(false);

            

            /* Update checkbox state based on camera value. */
            auto setupCheckbox = [](QCheckBox* checkBox, bool firstCamera, bool value){
                Qt::CheckState state = value ? Qt::Checked : Qt::Unchecked;
                if (firstCamera) {
                    checkBox->setTristate(false);
                    checkBox->setCheckState(state);
                }
                else if (state != checkBox->checkState()) {
                    checkBox->setTristate(true);
                    checkBox->setCheckState(Qt::PartiallyChecked);
                }
            };

            bool firstCamera = true; 
            QSet<QString> logins, passwords;
            bool audioSupported = false;
            bool sameArOverride = true;
            qreal arOverride;
            QString rotFirst;
            bool sameRotation = true;
            for (const QnVirtualCameraResourcePtr &camera: m_cameras) {
                logins.insert(camera->getAuth().user());
                passwords.insert(camera->getAuth().password());

                audioSupported |= camera->isAudioSupported();

                if (camera->isDtsBased()) {
                    ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, false);
                    ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, false);
                    ui->analogGroupBox->setVisible(true);
                }

                setupCheckbox(ui->analogViewCheckBox,   firstCamera, !camera->isScheduleDisabled());
                setupCheckbox(ui->enableAudioCheckBox,  firstCamera, camera->isAudioEnabled());
                setupCheckbox(ui->fisheyeCheckBox,      firstCamera, camera->getDewarpingParams().enabled);
      
                qreal changedAr = camera->customAspectRatio();
                if (firstCamera) {
                    arOverride = changedAr;
                } else {
                    sameArOverride &= qFuzzyEquals(changedAr, arOverride);
                }

                QString rotation = camera->getProperty(QnMediaResource::rotationKey());
                if(firstCamera) {
                    rotFirst = rotation;
                } else {
                    sameRotation &= rotFirst == rotation;
                }

                firstCamera = false;
            }

            ui->enableAudioCheckBox->setEnabled(audioSupported);
            ui->arOverrideCheckBox->setTristate(!sameArOverride);
            if (sameArOverride) {
                ui->arOverrideCheckBox->setChecked(!qFuzzyIsNull(arOverride));

                /* Float is important here. */
                float ar = QnAspectRatio::closestStandardRatio(arOverride).toFloat();
                int idx = -1;
                for (int i = 0; i < ui->arComboBox->count(); ++i) {
                    if (qFuzzyEquals(ar, ui->arComboBox->itemData(i).toFloat())) {
                        idx = i;
                        break;
                    }
                }
                ui->arComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
            } else {
                ui->arComboBox->setCurrentIndex(0);
                ui->arOverrideCheckBox->setCheckState(Qt::PartiallyChecked);
            }

            ui->rotCheckBox->setTristate(!sameRotation);
            if(sameRotation) {
                 ui->rotCheckBox->setChecked(!rotFirst.isEmpty());

                 int degree = rotFirst.toInt();
                 ui->rotComboBox->setCurrentIndex(degree/90);
                 int idx = -1;
                 for (int i = 0; i < ui->rotComboBox->count(); ++i) {
                     if (ui->rotComboBox->itemData(i).toInt() == degree) {
                         idx = i;
                         break;
                     }
                 }
                 ui->rotComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
            } else {
                ui->rotComboBox->setCurrentIndex(0);
                ui->rotCheckBox->setCheckState(Qt::PartiallyChecked);
            }

            ui->expertSettingsWidget->updateFromResources(m_cameras);

            bool isScheduleEqual = true;
            QList<QnScheduleTask::Data> scheduleTasksData;
            foreach (const QnScheduleTask& scheduleTask, m_cameras.front()->getScheduleTasks())
                scheduleTasksData << scheduleTask.getData();

            for (const QnVirtualCameraResourcePtr &camera: m_cameras) {
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
            for (const QnVirtualCameraResourcePtr &camera: m_cameras) 
                isMotionAvailable &= camera->hasMotion();
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
        updateLicenseText();
    }

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
    setReadOnly(ui->fisheyeCheckBox, readOnly);
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
    Qt::CheckState state = ui->enableAudioCheckBox->checkState();

    ui->enableAudioCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->enableAudioCheckBox->setCheckState(Qt::Checked);
}

void QnMultipleCameraSettingsWidget::at_fisheyeCheckBox_clicked() {
    Qt::CheckState state = ui->fisheyeCheckBox->checkState();

    ui->fisheyeCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->fisheyeCheckBox->setCheckState(Qt::Checked);
}

void QnMultipleCameraSettingsWidget::at_analogViewCheckBox_clicked() {
    Qt::CheckState state = ui->analogViewCheckBox->checkState();

    ui->analogViewCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->analogViewCheckBox->setCheckState(Qt::Checked);
    ui->cameraScheduleWidget->setScheduleEnabled(ui->analogViewCheckBox->isChecked());
}

void QnMultipleCameraSettingsWidget::updateLicensesButtonVisible() {
    ui->moreLicensesButton->setVisible(context()->accessController()->globalPermissions() & Qn::GlobalProtectedPermission);
}

void QnMultipleCameraSettingsWidget::updateLicenseText() {
    QnCamLicenseUsageHelper helper;

    switch(ui->analogViewCheckBox->checkState()) {
    case Qt::Checked:
        helper.propose(m_cameras, true);
        break;
    case Qt::Unchecked:
        helper.propose(m_cameras, false);
        break;
    default:
        break;
    }

    ui->licensesUsageWidget->loadData(&helper);
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
