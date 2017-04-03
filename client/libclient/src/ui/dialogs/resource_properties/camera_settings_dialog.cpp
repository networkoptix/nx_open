#include "camera_settings_dialog.h"

#include <QtGui/QWindow>
#include <QtGui/QScreen>

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

namespace {

static const int kMinimumWidth = 900; //< do not set minimum height
static const QSize kOptimalSize(900, 880);

/* Initial dialog height - 40px less than screen height. */
static const int kSizeOffset = 40;

}

QnCameraSettingsDialog::QnCameraSettingsDialog(QWidget *parent):
    base_type(parent),
    m_ignoreAccept(false)
{
    setMinimumWidth(kMinimumWidth);

    int maximumHeight = mainWindow()->geometry().height();
    if (auto windowHandle = mainWindow()->windowHandle())
    {
        if (auto currentScreen = windowHandle->screen())
            maximumHeight = currentScreen->size().height();
    }
    int optimalHeight = std::min(kOptimalSize.height(),
        maximumHeight - kSizeOffset);

    QSize optimalSize(kOptimalSize.width(), optimalHeight);
    QRect targetGeometry(QPoint(0, 0), optimalSize);
    targetGeometry.moveCenter(mainWindow()->geometry().center());
    setGeometry(targetGeometry);

    m_settingsWidget = new QnCameraSettingsWidget(this);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_applyButton = m_buttonBox->button(QDialogButtonBox::Apply);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);

    m_openButton = new QPushButton(tr("Show on Layout"));
    m_buttonBox->addButton(m_openButton, QDialogButtonBox::HelpRole);

    m_diagnoseButton = new QPushButton(tr("Event Log..."));
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

    static const QString kWindowTitlePattern = lit("%1 - %2");

    const QString caption = QnDeviceDependentStrings::getNameFromSet(QnCameraDeviceStringSet(
        resourcePool(),
        tr("Device Settings"), tr("Devices Settings"),
        tr("Camera Settings"), tr("Cameras Settings"),
        tr("I/O Module Settings"), tr("I/O Modules Settings")
    ), cameras);

    const QString description = cameras.size() == 1
        ? cameras.first()->getName()
        : QnDeviceDependentStrings::getNumericName(cameras);

    setWindowTitle(kWindowTitlePattern.arg(caption).arg(description));

    const QString rulesTitle = QnDeviceDependentStrings::getNameFromSet(QnCameraDeviceStringSet(
        resourcePool(),
        tr("Device Rules..."), tr("Devices Rules..."),
        tr("Camera Rules..."), tr("Cameras Rules..."),
        tr("I/O Module Rules..."), tr("I/O Modules Rules...")
    ), cameras);

    m_rulesButton->setText(rulesTitle);
}


bool QnCameraSettingsDialog::tryClose(bool force)
{
    auto result = setCameras(QnVirtualCameraResourceList(), force);
    result |= force;
    if (result)
        hide();

    return result;
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
    m_applyButton->setEnabled(hasChanges && !commonModule()->isReadOnly());
    m_settingsWidget->setExportScheduleButtonEnabled(!hasChanges);
}

void QnCameraSettingsDialog::at_settingsWidget_modeChanged()
{
    QnCameraSettingsWidget::Mode mode = m_settingsWidget->mode();
    bool isValidMode = (mode == QnCameraSettingsWidget::SingleMode || mode == QnCameraSettingsWidget::MultiMode);
    m_okButton->setEnabled(isValidMode && !commonModule()->isReadOnly());
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
            submitToResources();
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

bool QnCameraSettingsDialog::setCameras(const QnVirtualCameraResourceList& cameras, bool force)
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

        const auto extras = QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
            tr("Changes to the following %n devices are not saved:", "", unsavedCameras.size()),
            tr("Changes to the following %n cameras are not saved:", "", unsavedCameras.size()),
            tr("Changes to the following %n I/O Modules are not saved:", "", unsavedCameras.size())
        ), unsavedCameras);

        QnMessageBox messageBox(QnMessageBoxIcon::Question,
            tr("Apply changes before switching to another camera?"), extras,
            QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
            QDialogButtonBox::Apply, mainWindow());

        messageBox.addCustomWidget(new QnResourceListView(unsavedCameras, &messageBox));
        const auto result = messageBox.exec();
        switch (result)
        {
            case QDialogButtonBox::Apply:
                submitToResources();
                break;
            case QDialogButtonBox::Discard:
                break;
            default:
                /* Cancel changes. */
                return false;
        }

    }

    m_settingsWidget->setCameras(cameras);
    retranslateUi();
    return true;
}

Qn::CameraSettingsTab QnCameraSettingsDialog::currentTab() const
{
    return m_settingsWidget->currentTab();
}

void QnCameraSettingsDialog::setCurrentTab(Qn::CameraSettingsTab tab)
{
    m_settingsWidget->setCurrentTab(tab);
}

void QnCameraSettingsDialog::submitToResources()
{
    bool hasDbChanges = m_settingsWidget->hasDbChanges();

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

    /* Submit and save it. */
    saveCameras(cameras);
}

void QnCameraSettingsDialog::updateFromResources()
{
    m_settingsWidget->updateFromResources();
}

void QnCameraSettingsDialog::saveCameras(const QnVirtualCameraResourceList &cameras)
{
    if (cameras.isEmpty())
        return;

    auto applyChanges = [this, cameras]
        {
            m_settingsWidget->submitToResources();
            for (const QnVirtualCameraResourcePtr &camera : cameras)
                if (camera->preferredServerId().isNull())
                    camera->setPreferredServerId(camera->getParentId());
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
