#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <limits>

#include <QtCore/QScopedValueRollback>

#include <QtWidgets/QPushButton>

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
#include <ui/workbench/workbench_context.h>

#include "camera_schedule_widget.h"
#include "camera_motion_mask_widget.h"

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

MultipleCameraSettingsWidget::MultipleCameraSettingsWidget(QWidget *parent):
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
    ui->licensingWidget->initializeContext();
    ui->cameraScheduleWidget->initializeContext();

    CheckboxUtils::autoClearTristate(ui->enableAudioCheckBox);

    connect(ui->loginEdit, &QLineEdit::textChanged, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->enableAudioCheckBox, &QCheckBox::stateChanged, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->passwordEdit, &QLineEdit::textChanged, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::scheduleTasksChanged, this,
        &MultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged);
    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::recordingSettingsChanged, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::controlsChangesApplied, this,
        [this] { m_hasScheduleControlsChanges = false; });
    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::gridParamsChanged, this,
        [this] { m_hasScheduleControlsChanges = true; });
    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::scheduleEnabledChanged, this,
        &MultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged);
    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::archiveRangeChanged, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &CameraScheduleWidget::alert, this,
        [this](const QString& text) { m_alertText = text; updateAlertBar(); });

    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed, this,
        [this]
        {
            if (m_updating)
                return;
            ui->cameraScheduleWidget->setScheduleEnabled(ui->licensingWidget->state() == Qt::Checked);
        });

    connect(ui->imageControlWidget, &ImageControlWidget::changed, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->expertSettingsWidget, &CameraExpertSettingsWidget::dataChanged, this,
        &MultipleCameraSettingsWidget::at_dbDataChanged);


    /* Set up context help. */
    setHelpTopic(this, Qn::CameraSettings_Multi_Help);
    setHelpTopic(ui->tabRecording, Qn::CameraSettings_Recording_Help);

    updateFromResources();
}

MultipleCameraSettingsWidget::~MultipleCameraSettingsWidget()
{
    return;
}

const QnVirtualCameraResourceList &MultipleCameraSettingsWidget::cameras() const
{
    return m_cameras;
}

void MultipleCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    if (m_cameras == cameras)
        return;

    QScopedValueRollback<bool> updateRollback(m_updating, true);

    m_cameras = cameras;
    ui->cameraScheduleWidget->setCameras(m_cameras);
    ui->licensingWidget->setCameras(m_cameras);

    updateFromResources();
}

CameraSettingsTab MultipleCameraSettingsWidget::currentTab() const
{
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();

    if (tab == ui->tabGeneral)
        return CameraSettingsTab::general;

    if (tab == ui->tabRecording)
        return CameraSettingsTab::recording;

    if (tab == ui->expertTab)
        return CameraSettingsTab::expert;

    NX_ASSERT(false, "Should never get here");
    return CameraSettingsTab::general;
}

void MultipleCameraSettingsWidget::setCurrentTab(CameraSettingsTab tab)
{
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    if (!ui->tabWidget->isTabEnabled(tabIndex(tab)))
    {
        ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        return;
    }

    switch (tab)
    {
        case CameraSettingsTab::general:
        case CameraSettingsTab::io:
            ui->tabWidget->setCurrentWidget(ui->tabGeneral);
            break;
        case CameraSettingsTab::recording:
        case CameraSettingsTab::motion:
        case CameraSettingsTab::advanced:
            ui->tabWidget->setCurrentWidget(ui->tabRecording);
            break;
        case CameraSettingsTab::expert:

            ui->tabWidget->setCurrentWidget(ui->expertTab);
            break;
        default:
            qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
            ui->tabWidget->setCurrentWidget(ui->tabGeneral);
            break;
    }
}

void MultipleCameraSettingsWidget::updateAlertBar()
{
    if (currentTab() == CameraSettingsTab::recording)
        ui->alertBar->setText(m_alertText);
    else
        ui->alertBar->setText(QString());
}

void MultipleCameraSettingsWidget::submitToResources()
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

bool MultipleCameraSettingsWidget::isValidSecondStream()
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

bool MultipleCameraSettingsWidget::hasDbChanges() const
{
    return m_hasDbChanges;
}

void MultipleCameraSettingsWidget::updateFromResources()
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

        setTabEnabledSafe(CameraSettingsTab::io, !m_lockedMode);
        setTabEnabledSafe(CameraSettingsTab::motion, !m_lockedMode);
        setTabEnabledSafe(CameraSettingsTab::fisheye, !m_lockedMode);
        setTabEnabledSafe(CameraSettingsTab::advanced, !m_lockedMode);
        setTabEnabledSafe(CameraSettingsTab::recording, !isDtsBased && recordingSupported && !m_lockedMode);
        setTabEnabledSafe(CameraSettingsTab::expert, !isDtsBased && hasVideo && !m_lockedMode);
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

    setTabEnabledSafe(CameraSettingsTab::general, !m_lockedMode);
}

bool MultipleCameraSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void MultipleCameraSettingsWidget::setReadOnly(bool readOnly)
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

void MultipleCameraSettingsWidget::setLockedMode(bool locked)
{
    if (m_lockedMode == locked)
        return;

    m_lockedMode = locked;
    updateFromResources();
}

void MultipleCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->cameraScheduleWidget->setExportScheduleButtonEnabled(enabled);
}

void MultipleCameraSettingsWidget::setHasDbChanges(bool hasChanges)
{
    if (m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;

    emit hasChangesChanged();
}


int MultipleCameraSettingsWidget::tabIndex(CameraSettingsTab tab) const
{
    switch (tab)
    {
        case CameraSettingsTab::general:
            return ui->tabWidget->indexOf(ui->tabGeneral);
        case CameraSettingsTab::recording:
            return ui->tabWidget->indexOf(ui->tabRecording);
        case CameraSettingsTab::expert:
            return ui->tabWidget->indexOf(ui->expertTab);
        default:
            break;
    }

    /* Passing here is totally normal because we want to save the current tab while switching between dialog modes. */
    return ui->tabWidget->indexOf(ui->tabGeneral);
}

void MultipleCameraSettingsWidget::setTabEnabledSafe(CameraSettingsTab tab, bool enabled)
{
    if (!enabled && currentTab() == tab)
        setCurrentTab(CameraSettingsTab::general);
    ui->tabWidget->setTabEnabled(tabIndex(tab), enabled);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void MultipleCameraSettingsWidget::at_dbDataChanged()
{
    if (m_updating)
        return;

    setHasDbChanges(true);
}

void MultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged()
{
    if (m_updating)
        return;

    at_dbDataChanged();
}

void MultipleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged(int state)
{
    if (m_updating)
        return;

    ui->licensingWidget->setState(static_cast<Qt::CheckState>(state));
    at_dbDataChanged();
}

} // namespace desktop
} // namespace client
} // namespace nx
