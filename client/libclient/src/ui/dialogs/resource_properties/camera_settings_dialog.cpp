#include "camera_settings_dialog.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resources_changes_manager.h>

#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>

#include <ui/help/help_topics.h>
#include <ui/widgets/properties/camera_settings_widget.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>

#include <utils/license_usage_helper.h>

QnCameraSettingsDialog::QnCameraSettingsDialog(QWidget *parent):
    base_type(parent),
    m_ignoreAccept(false)
{
    m_settingsWidget = new QnCameraSettingsWidget(this);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_applyButton = m_buttonBox->button(QDialogButtonBox::Apply);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);

    m_openButton = new QPushButton(tr("Show on Layout"));
    m_buttonBox->addButton(m_openButton, QDialogButtonBox::HelpRole);

    m_diagnoseButton = new QPushButton(tr("Event Log"));
    m_buttonBox->addButton(m_diagnoseButton, QDialogButtonBox::HelpRole);

    m_rulesButton = new QPushButton();
    m_buttonBox->addButton(m_rulesButton, QDialogButtonBox::HelpRole);

    auto separator = new QFrame(this);
    separator->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_settingsWidget);
    layout->addWidget(separator);
    layout->addWidget(m_buttonBox);

    connect(m_settingsWidget, &QnCameraSettingsWidget::hasChangesChanged, this, &QnCameraSettingsDialog::at_settingsWidget_hasChangesChanged);
    connect(m_settingsWidget, &QnCameraSettingsWidget::modeChanged, this, &QnCameraSettingsDialog::at_settingsWidget_modeChanged);

    connect(m_openButton, &QPushButton::clicked, this, &QnCameraSettingsDialog::at_openButton_clicked);
    connect(m_diagnoseButton, &QPushButton::clicked, this, &QnCameraSettingsDialog::at_diagnoseButton_clicked);
    connect(m_rulesButton, &QPushButton::clicked, this, &QnCameraSettingsDialog::at_rulesButton_clicked);

    connect(m_settingsWidget, &QnCameraSettingsWidget::resourcesChanged, this, &QnCameraSettingsDialog::updateReadOnly);

    connect(context(), &QnWorkbenchContext::userChanged, this, &QnCameraSettingsDialog::updateReadOnly);

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this, [this](const QnResourceList &resources)
    {
        if (isHidden())
            return;

        auto cameras = resources.filtered<QnVirtualCameraResource>();
        if (!cameras.isEmpty())
            setCameras(cameras);
    });

    auto safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(m_buttonBox);
    safeModeWatcher->addControlledWidget(m_okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);
    safeModeWatcher->addControlledWidget(m_applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    at_settingsWidget_hasChangesChanged();
    retranslateUi();
}

QnCameraSettingsDialog::~QnCameraSettingsDialog()
{
}

void QnCameraSettingsDialog::retranslateUi()
{
    base_type::retranslateUi();

    auto cameras = m_settingsWidget->cameras();

    const QString windowTitle = QnDeviceDependentStrings::getNameFromSet(QnCameraDeviceStringSet(
        tr("Device Settings"), tr("Devices Settings"),
        tr("Camera Settings"), tr("Cameras Settings"),
        tr("I/O Module Settings"), tr("I/O Modules Settings")
    ), cameras);

    const QString rulesTitle = QnDeviceDependentStrings::getNameFromSet(QnCameraDeviceStringSet(
        tr("Device Rules"), tr("Devices Rules"),
        tr("Camera Rules"), tr("Cameras Rules"),
        tr("I/O Module Rules"), tr("I/O Modules Rules")
    ), cameras);

    setWindowTitle(windowTitle);
    m_rulesButton->setText(rulesTitle);
}


bool QnCameraSettingsDialog::tryClose(bool force)
{
    setCameras(QnVirtualCameraResourceList(), force);
    if (force)
        hide();
    return true;
}


void QnCameraSettingsDialog::accept()
{
    if (m_ignoreAccept)
    {
        m_ignoreAccept = false;
        return;
    }
    base_type::accept();
}

void QnCameraSettingsDialog::reject()
{
    m_settingsWidget->reject();
    base_type::reject();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraSettingsDialog::at_settingsWidget_hasChangesChanged()
{
    bool hasChanges = m_settingsWidget->hasDbChanges();
    m_applyButton->setEnabled(hasChanges && !qnCommon->isReadOnly());
    m_settingsWidget->setExportScheduleButtonEnabled(!hasChanges);
}

void QnCameraSettingsDialog::at_settingsWidget_modeChanged()
{
    QnCameraSettingsWidget::Mode mode = m_settingsWidget->mode();
    bool isValidMode = (mode == QnCameraSettingsWidget::SingleMode || mode == QnCameraSettingsWidget::MultiMode);
    m_okButton->setEnabled(isValidMode && !qnCommon->isReadOnly());
    m_openButton->setVisible(isValidMode);
    m_diagnoseButton->setVisible(isValidMode);
    m_rulesButton->setVisible(mode == QnCameraSettingsWidget::SingleMode);  //TODO: #GDM implement
}

void QnCameraSettingsDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    switch (button)
    {
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

void QnCameraSettingsDialog::at_diagnoseButton_clicked()
{
    menu()->trigger(QnActions::CameraIssuesAction, m_settingsWidget->cameras());
}

void QnCameraSettingsDialog::at_rulesButton_clicked()
{
    menu()->trigger(QnActions::CameraBusinessRulesAction, m_settingsWidget->cameras());
}

void QnCameraSettingsDialog::updateReadOnly()
{
    Qn::Permissions permissions = accessController()->combinedPermissions(m_settingsWidget->cameras());
    m_settingsWidget->setReadOnly(!permissions.testFlag(Qn::WritePermission));
}

void QnCameraSettingsDialog::setCameras(const QnVirtualCameraResourceList& cameras, bool force)
{
    bool askConfirmation =
        !force
        &&  isVisible()
        && m_settingsWidget->cameras() != cameras
        && !m_settingsWidget->cameras().isEmpty()
        && (m_settingsWidget->hasDbChanges());

    if (askConfirmation)
    {
        auto unsavedCameras = m_settingsWidget->cameras();

        const auto question = QnDeviceDependentStrings::getNameFromSet(QnCameraDeviceStringSet(
            tr("Apply changes to the following %n devices?", "", unsavedCameras.size()),
            tr("Apply changes to the following %n cameras?", "", unsavedCameras.size()),
            tr("Apply changes to the following %n I/O modules?", "", unsavedCameras.size())
        ), unsavedCameras);

        QnMessageBox messageBox(
            QnMessageBox::Warning,
            Qn::Empty_Help,
            tr("Changes are not saved"),
            tr("Changes are not saved"),
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            mainWindow());
        messageBox.setDefaultButton(QDialogButtonBox::Yes);
        messageBox.setInformativeText(question);
        messageBox.addCustomWidget(new QnResourceListView(unsavedCameras));
        auto result = messageBox.exec();
        switch (result)
        {
            case QDialogButtonBox::Yes:
                submitToResources();
                break;
            case QDialogButtonBox::No:
                break;
            default:
                /* Cancel changes. */
                return;
        }

    }

    m_settingsWidget->setCameras(cameras);
    retranslateUi();
}

void QnCameraSettingsDialog::submitToResources(bool checkControls /* = false*/)
{
    bool hasDbChanges = m_settingsWidget->hasDbChanges();

    if (checkControls && m_settingsWidget->hasScheduleControlsChanges())
    {
        QString message = tr("Recording settings have not been saved. Please choose desired recording method, FPS, and quality - then mark the changes on the schedule.");
        int button = QnMessageBox::warning(
            this, tr("Changes have not been applied."),
            message,
            QDialogButtonBox::Retry | QDialogButtonBox::Ignore,
            QDialogButtonBox::Ignore);
        if (button == QDialogButtonBox::Retry)
        {
            m_ignoreAccept = true;
            return;
        }
        else
        {
            m_settingsWidget->clearScheduleControlsChanges();
        }
    }
    else if (checkControls && m_settingsWidget->hasMotionControlsChanges())
    {
        QString message = tr("Motion sensitivity has not changed. To change motion sensitivity draw rectangle on the image.");
        int button = QnMessageBox::warning(
            this, tr("Changes have not been applied."),
            message,
            QDialogButtonBox::Retry | QDialogButtonBox::Ignore,
            QDialogButtonBox::Ignore);
        if (button == QDialogButtonBox::Retry)
        {
            m_ignoreAccept = true;
            return;
        }
        else
        {
            m_settingsWidget->clearMotionControlsChanges();
        }
    }

    if (!hasDbChanges)
        return;

    QnVirtualCameraResourceList cameras = m_settingsWidget->cameras();
    if (cameras.empty())
        return;

    /* Dialog will be shown inside */
    if (!m_settingsWidget->isValidMotionRegion())
    {
        m_ignoreAccept = true;
        return;
    }

    /* Dialog will be shown inside */
    if (!m_settingsWidget->isValidSecondStream())
    {
        m_ignoreAccept = true;
        return;
    }

    //checking if showing Licenses limit exceeded is appropriate
    if (m_settingsWidget->licensedParametersModified())
    {
        QnCamLicenseUsageHelper helper(cameras, m_settingsWidget->isScheduleEnabled());
        if (!helper.isValid())
        {
            QString message = tr("License limit exceeded. Changes have been saved, but will not be applied.");
            QnMessageBox::warning(this, tr("Could not apply changes."), message);
            m_settingsWidget->setScheduleEnabled(false);
        }
    }

    /* Submit and save it. */
    saveCameras(cameras);
}

void QnCameraSettingsDialog::saveCameras(const QnVirtualCameraResourceList &cameras)
{
    if (cameras.isEmpty())
        return;

    auto applyChanges = [this, cameras]
        {
            m_settingsWidget->submitToResources();
            for (const QnVirtualCameraResourcePtr &camera : cameras)
                if (camera->preferedServerId().isNull())
                    camera->setPreferedServerId(camera->getParentId());
        };

    auto rollback = [this, cameras]()
        {
            if (!isVisible())
                return;

            if (m_settingsWidget->cameras() != cameras)
                return;

            m_settingsWidget->updateFromResources();
        };

    qnResourcesChangesManager->saveCamerasBatch(cameras, applyChanges, rollback);
}

void QnCameraSettingsDialog::at_openButton_clicked()
{
    QnVirtualCameraResourceList cameras = m_settingsWidget->cameras();
    menu()->trigger(QnActions::OpenInNewLayoutAction, cameras);
    m_settingsWidget->setCameras(cameras);
    retranslateUi();
}
