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
#include <ui/common/checkbox_utils.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/licensing/licenses_propose_widget.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/widgets/properties/camera_settings_widget_p.h>
#include <ui/workbench/workbench_context.h>

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
    ui->licensingWidget->initializeContext(this);
    ui->cameraScheduleWidget->initializeContext(this);

    QnCheckbox::autoCleanTristate(ui->enableAudioCheckBox);

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));

    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));

    connect(ui->cameraScheduleWidget,   &QnCameraScheduleWidget::recordingSettingsChanged,      this,   &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget,   &QnCameraScheduleWidget::recordingSettingsChanged,      this,   [this] {m_hasScheduleChanges = true;});
    connect(ui->cameraScheduleWidget,   &QnCameraScheduleWidget::controlsChangesApplied,        this,   [this] {m_hasScheduleControlsChanges = false;});
    connect(ui->cameraScheduleWidget,   &QnCameraScheduleWidget::gridParamsChanged,             this,   [this] {m_hasScheduleControlsChanges = true;});

    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged(int)));

    connect(ui->licensingWidget,         &QnLicensesProposeWidget::changed,      this,   &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->licensingWidget,         &QnLicensesProposeWidget::changed,      this,   [this] {
        ui->cameraScheduleWidget->setScheduleEnabled(ui->licensingWidget->state() == Qt::Checked);
    });

    connect(ui->imageControlWidget,     &QnImageControlWidget::changed,         this,   &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->expertSettingsWidget,   SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(archiveRangeChanged()),          this,   SLOT(at_dbDataChanged()));

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
    if (isReadOnly())
        return;

    QString login = ui->loginEdit->text().trimmed();
    QString password = ui->passwordEdit->text().trimmed();

    QnScheduleTaskList scheduleTasks;
    if(m_hasScheduleChanges)
        foreach(const QnScheduleTask::Data &data, ui->cameraScheduleWidget->scheduleTasks())
            scheduleTasks.append(QnScheduleTask(data));

    for(const QnVirtualCameraResourcePtr &camera: m_cameras) 
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

        if (m_hasScheduleEnabledChanges) {
            camera->setLicenseUsed(ui->cameraScheduleWidget->isScheduleEnabled());
            camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());
        }

        if (m_hasScheduleChanges)
            camera->setScheduleTasks(scheduleTasks);

    }

    ui->imageControlWidget->submitToResources(m_cameras);
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
        tr("Invalid Schedule"),
        tr("Second stream is disabled on this camera. Motion + LQ option has no effect. "\
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
        ui->imageControlWidget->updateFromResources(m_cameras);
        ui->licensingWidget->setCameras(m_cameras);

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

            using boost::algorithm::any_of;
            using boost::algorithm::all_of;

            const bool isDtsBased        = any_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->isDtsBased(); });
            const bool hasVideo          = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->hasVideo(0); });
            const bool audioSupported    = any_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->isAudioSupported(); });
            const bool audioForced       = any_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->isAudioForced(); });
            const bool isMotionAvailable = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->hasMotion(); });

            const bool audioEnabled      = m_cameras.front()->isAudioEnabled();
            const bool sameAudioEnabled  = all_of(m_cameras, [audioEnabled](const QnVirtualCameraResourcePtr &camera) {
                return camera->isAudioEnabled() == audioEnabled; 
            });

            ui->cameraScheduleWidget->setMotionAvailable(isMotionAvailable);
            ui->enableAudioCheckBox->setEnabled(audioSupported && !audioForced);            
            ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, !isDtsBased);
            ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, !isDtsBased && hasVideo);
            ui->expertTab->setEnabled(!isDtsBased && hasVideo);
            QnCheckbox::setupTristateCheckbox(ui->enableAudioCheckBox, sameAudioEnabled, audioEnabled);

            {           
                QSet<QString> logins, passwords;

                for (const QnVirtualCameraResourcePtr &camera: m_cameras) {
                    logins.insert(camera->getAuth().user());
                    passwords.insert(camera->getAuth().password());
                }          

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

            ui->expertSettingsWidget->updateFromResources(m_cameras);

            int maxFps = std::numeric_limits<int>::max();
            int maxDualStreamingFps = maxFps;

            Q_D(QnCameraSettingsWidget);
            d->calculateMaxFps(&maxFps, &maxDualStreamingFps);

            bool isScheduleEqual = true;
            QList<QnScheduleTask::Data> scheduleTasksData;
            foreach (const QnScheduleTask& scheduleTask, m_cameras.front()->getScheduleTasks())
                scheduleTasksData << scheduleTask.getData();

            for (const QnVirtualCameraResourcePtr &camera: m_cameras) {
                QList<QnScheduleTask::Data> cameraScheduleTasksData;
                foreach (const QnScheduleTask& scheduleTask, camera->getScheduleTasks()) {
                    cameraScheduleTasksData << scheduleTask.getData();

                    bool fpsValid = true;
                    switch (scheduleTask.getRecordingType()) {
                    case Qn::RT_Never:
                        continue;
                    case Qn::RT_MotionAndLowQuality:
                        fpsValid = scheduleTask.getFps() <= maxDualStreamingFps;
                        break;
                    case Qn::RT_Always:
                    case Qn::RT_MotionOnly:
                        fpsValid = scheduleTask.getFps() <= maxFps;
                        break;
                    default:
                        break;
                    }

                    if (!fpsValid) {
                        isScheduleEqual = false;
                        break;
                    }
                }

                if (!isScheduleEqual || cameraScheduleTasksData != scheduleTasksData) {
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

        }

        ui->cameraScheduleWidget->setCameras(m_cameras);
        
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
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    setReadOnly(ui->imageControlWidget, readOnly);
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

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged(int state) {
    ui->licensingWidget->setState(static_cast<Qt::CheckState>(state));
    at_dbDataChanged();
    m_hasScheduleEnabledChanges = true;
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
