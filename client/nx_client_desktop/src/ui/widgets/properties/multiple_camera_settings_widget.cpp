#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <limits>

#include <QtCore/QScopedValueRollback>

#include <QtWidgets/QPushButton>

// TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/ui/common/checkbox_utils.h>
#include <ui/common/read_only.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/licensing/licenses_propose_widget.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::client::desktop::ui;

QnMultipleCameraSettingsWidget::QnMultipleCameraSettingsWidget(QWidget *parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::MultipleCameraSettingsWidget),
    m_hasDbChanges(false),
    m_loginWasEmpty(true),
    m_passwordWasEmpty(true),
    m_hasScheduleControlsChanges(false),
    m_readOnly(false),
    m_updating(false)
{
    ui->setupUi(this);
    ui->licensingWidget->initializeContext(this);
    ui->cameraScheduleWidget->initializeContext(this);

    CheckboxUtils::autoClearTristate(ui->enableAudioCheckBox);

    connect(ui->loginEdit, &QLineEdit::textChanged, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->enableAudioCheckBox, &QCheckBox::stateChanged, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->passwordEdit, &QLineEdit::textChanged, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::scheduleTasksChanged, this,
        &QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::recordingSettingsChanged, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::controlsChangesApplied, this,
        [this] { m_hasScheduleControlsChanges = false; });
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::gridParamsChanged, this,
        [this] { m_hasScheduleControlsChanges = true; });
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::scheduleEnabledChanged, this,
        &QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::archiveRangeChanged, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::alert, this,
        [this](const QString& text) { m_alertText = text; updateAlertBar(); });

    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed, this,
        [this]
        {
            if (m_updating)
                return;
            ui->cameraScheduleWidget->setScheduleEnabled(ui->licensingWidget->state() == Qt::Checked);
        });

    connect(ui->imageControlWidget, &QnImageControlWidget::changed, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->expertSettingsWidget, &QnCameraExpertSettingsWidget::dataChanged, this,
        &QnMultipleCameraSettingsWidget::at_dbDataChanged);


    /* Set up context help. */
    setHelpTopic(this, Qn::CameraSettings_Multi_Help);
    setHelpTopic(ui->tabRecording, Qn::CameraSettings_Recording_Help);

    updateFromResources();
}

QnMultipleCameraSettingsWidget::~QnMultipleCameraSettingsWidget()
{
    return;
}

const QnVirtualCameraResourceList &QnMultipleCameraSettingsWidget::cameras() const
{
    return m_cameras;
}

void QnMultipleCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    if (m_cameras == cameras)
        return;

    QScopedValueRollback<bool> updateRollback(m_updating, true);

    m_cameras = cameras;
    ui->cameraScheduleWidget->setCameras(m_cameras);
    ui->licensingWidget->setCameras(m_cameras);

    updateFromResources();
}

Qn::CameraSettingsTab QnMultipleCameraSettingsWidget::currentTab() const
{
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();

    if (tab == ui->tabGeneral)
        return Qn::GeneralSettingsTab;

    if (tab == ui->tabRecording)
        return Qn::RecordingSettingsTab;

    if (tab == ui->expertTab)
        return Qn::ExpertCameraSettingsTab;

    qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
    return Qn::GeneralSettingsTab;
}

void QnMultipleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab)
{
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    if (!ui->tabWidget->isTabEnabled(tabIndex(tab)))
    {
        ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        return;
    }

    switch (tab)
    {
        case Qn::GeneralSettingsTab:
        case Qn::IOPortsSettingsTab:
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
            ui->tabWidget->setCurrentWidget(ui->tabGeneral);
            break;
    }
}

void QnMultipleCameraSettingsWidget::updateAlertBar()
{
    if (currentTab() == Qn::RecordingSettingsTab)
        ui->alertBar->setText(m_alertText);
    else
        ui->alertBar->setText(QString());
}

void QnMultipleCameraSettingsWidget::submitToResources()
{
    if (isReadOnly())
        return;

    QString login = ui->loginEdit->text().trimmed();
    QString password = ui->passwordEdit->text().trimmed();

    for (const auto& camera : m_cameras)
    {
        QAuthenticator auth = camera->getAuth();

        QString cameraLogin = auth.user();
        if (!login.isEmpty() || !m_loginWasEmpty)
            cameraLogin = login;

        QString cameraPassword = auth.password();
        if (!password.isEmpty() || !m_passwordWasEmpty)
            cameraPassword = password;

        camera->setAuth(cameraLogin, cameraPassword);

        if (ui->enableAudioCheckBox->checkState() != Qt::PartiallyChecked && camera->isAudioSupported())
            camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());
    }

    ui->cameraScheduleWidget->submitToResources();
    ui->imageControlWidget->submitToResources(m_cameras);
    ui->expertSettingsWidget->submitToResources(m_cameras);

    setHasDbChanges(false);
}

bool QnMultipleCameraSettingsWidget::isValidSecondStream()
{
    /* Do not check validness if there is no recording anyway. */
    if (!ui->cameraScheduleWidget->isScheduleEnabled())
        return true;

    auto filteredTasks = ui->cameraScheduleWidget->scheduleTasks();
    bool usesSecondStream = false;
    for (auto& task : filteredTasks)
    {
        if (task.getRecordingType() == Qn::RT_MotionAndLowQuality)
        {
            usesSecondStream = true;
            task.setRecordingType(Qn::RT_Always);
        }
    }

    /* There are no Motion+LQ tasks. */
    if (!usesSecondStream)
        return true;

    if (ui->expertSettingsWidget->isSecondStreamEnabled())
        return true;

    QnMessageBox dialog(QnMessageBoxIcon::Warning,
        tr("Secondary stream disabled for these cameras"),
        tr("\"Motion + Low - Res\" recording option cannot be set."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);

    const auto recordAlways = dialog.addButton(
        tr("Set Recording to \"Always\""), QDialogButtonBox::YesRole);
    dialog.addButton(
        tr("Enable Secondary Stream"), QDialogButtonBox::NoRole);

    dialog.setButtonAutoDetection(QnButtonDetection::NoDetection);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return false;

    if (dialog.clickedButton() == recordAlways)
    {
        ui->cameraScheduleWidget->setScheduleTasks(filteredTasks);
        return true;
    }

    ui->expertSettingsWidget->setSecondStreamEnabled();
    return true;
}

bool QnMultipleCameraSettingsWidget::hasDbChanges() const
{
    return m_hasDbChanges;
}

void QnMultipleCameraSettingsWidget::updateFromResources()
{
    m_alertText = QString();
    updateAlertBar();

    ui->imageControlWidget->updateFromResources(m_cameras);
    ui->licensingWidget->updateFromResources();
    ui->cameraScheduleWidget->updateFromResources();

    if (m_cameras.empty())
    {
        ui->loginEdit->setText(QString());
        ui->enableAudioCheckBox->setCheckState(Qt::Unchecked);

        ui->loginEdit->setPlaceholderText(QString());
        ui->passwordEdit->setText(QString());
        ui->passwordEdit->setPlaceholderText(QString());
    }
    else
    {
        /* Aggregate camera parameters first. */
        using boost::algorithm::any_of;
        using boost::algorithm::all_of;

        const bool isDtsBased = any_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->isDtsBased(); });
        const bool hasVideo = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });
        const bool audioSupported = any_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->isAudioSupported(); });
        const bool audioForced = any_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->isAudioForced(); });

        const bool recordingSupported = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0) || camera->isAudioSupported(); });

        const bool audioEnabled = m_cameras.front()->isAudioEnabled();
        const bool sameAudioEnabled = all_of(m_cameras,
            [audioEnabled](const QnVirtualCameraResourcePtr &camera)
            {
                return camera->isAudioEnabled() == audioEnabled;
            });

        ui->enableAudioCheckBox->setEnabled(audioSupported && !audioForced);

        setTabEnabledSafe(Qn::IOPortsSettingsTab, !m_lockedMode);
        setTabEnabledSafe(Qn::MotionSettingsTab, !m_lockedMode);
        setTabEnabledSafe(Qn::FisheyeCameraSettingsTab, !m_lockedMode);
        setTabEnabledSafe(Qn::AdvancedCameraSettingsTab, !m_lockedMode);
        setTabEnabledSafe(Qn::RecordingSettingsTab, !isDtsBased && recordingSupported && !m_lockedMode);
        setTabEnabledSafe(Qn::ExpertCameraSettingsTab, !isDtsBased && hasVideo && !m_lockedMode);
        CheckboxUtils::setupTristateCheckbox(ui->enableAudioCheckBox, sameAudioEnabled, audioEnabled);

        {
            QSet<QString> logins, passwords;

            for (const auto &camera : m_cameras)
            {
                QAuthenticator auth = camera->getAuth();
                logins.insert(auth.user());
                passwords.insert(auth.password());
            }

            if (logins.size() == 1)
            {
                ui->loginEdit->setText(*logins.begin());
                ui->loginEdit->setPlaceholderText(QString());
            }
            else
            {
                ui->loginEdit->setText(QString());
                ui->loginEdit->setPlaceholderText(L'<' + tr("multiple values") + L'>');
            }
            m_loginWasEmpty = ui->loginEdit->text().isEmpty();

            if (passwords.size() == 1)
            {
                ui->passwordEdit->setText(*passwords.begin());
                ui->passwordEdit->setPlaceholderText(QString());
            }
            else
            {
                ui->passwordEdit->setText(QString());
                ui->passwordEdit->setPlaceholderText(L'<' + tr("multiple values") + L'>');
            }
            m_passwordWasEmpty = ui->passwordEdit->text().isEmpty();
        }

        ui->expertSettingsWidget->updateFromResources(m_cameras);
    }

    setHasDbChanges(false);
    m_hasScheduleControlsChanges = false;

    ui->tabWidget->widget(Qn::GeneralSettingsTab)->setEnabled(!m_lockedMode);
}

bool QnMultipleCameraSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnMultipleCameraSettingsWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->enableAudioCheckBox, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    setReadOnly(ui->imageControlWidget, readOnly);
    m_readOnly = readOnly;
}

void QnMultipleCameraSettingsWidget::setLockedMode(bool locked)
{
    if (m_lockedMode == locked)
        return;

    m_lockedMode = locked;
    updateFromResources();
}

void QnMultipleCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->cameraScheduleWidget->setExportScheduleButtonEnabled(enabled);
}

void QnMultipleCameraSettingsWidget::setHasDbChanges(bool hasChanges)
{
    if (m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;

    emit hasChangesChanged();
}


int QnMultipleCameraSettingsWidget::tabIndex(Qn::CameraSettingsTab tab) const
{
    switch (tab)
    {
        case Qn::GeneralSettingsTab:
            return ui->tabWidget->indexOf(ui->tabGeneral);
        case Qn::RecordingSettingsTab:
            return ui->tabWidget->indexOf(ui->tabRecording);
        case Qn::ExpertCameraSettingsTab:
            return ui->tabWidget->indexOf(ui->expertTab);
        default:
            break;
    }

    /* Passing here is totally normal because we want to save the current tab while switching between dialog modes. */
    return ui->tabWidget->indexOf(ui->tabGeneral);
}

void QnMultipleCameraSettingsWidget::setTabEnabledSafe(Qn::CameraSettingsTab tab, bool enabled)
{
    if (!enabled && currentTab() == tab)
        setCurrentTab(Qn::GeneralSettingsTab);
    ui->tabWidget->setTabEnabled(tabIndex(tab), enabled);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnMultipleCameraSettingsWidget::at_dbDataChanged()
{
    if (m_updating)
        return;

    setHasDbChanges(true);
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged()
{
    if (m_updating)
        return;

    at_dbDataChanged();
}

void QnMultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged(int state)
{
    if (m_updating)
        return;

    ui->licensingWidget->setState(static_cast<Qt::CheckState>(state));
    at_dbDataChanged();
}
