#include "camera_settings_dialog.h"
#include "ui_camera_settings_dialog.h"

#include <QtCore/QSharedPointer>
#include <QtWidgets/QPushButton>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/common/read_only.h>
#include <utils/common/html.h>
#include <utils/common/event_processors.h>
#include <utils/license_usage_helper.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <client_core/client_core_module.h>

#include "camera_settings_tab.h"
#include "redux/camera_settings_dialog_state.h"
#include "redux/camera_settings_dialog_store.h"
#include "utils/camera_settings_dialog_state_conversion_functions.h"
#include "utils/license_usage_provider.h"
#include "utils/device_agent_settings_adapter.h"
#include "watchers/camera_settings_license_watcher.h"
#include "watchers/camera_settings_readonly_watcher.h"
#include "watchers/camera_settings_analytics_engines_watcher.h"
#include "watchers/camera_settings_wearable_state_watcher.h"
#include "watchers/camera_settings_global_settings_watcher.h"
#include "watchers/camera_settings_global_permissions_watcher.h"
#include "widgets/camera_settings_general_tab_widget.h"
#include "widgets/camera_schedule_widget.h"
#include "widgets/camera_motion_settings_widget.h"
#include "widgets/camera_fisheye_settings_widget.h"
#include "widgets/camera_expert_settings_widget.h"
#include "widgets/camera_analytics_settings_widget.h"
#include "widgets/camera_web_page_widget.h"
#include "widgets/io_module_settings_widget.h"
#include "camera_advanced_settings_widget.h"

#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialog::Private: public QObject
{
    CameraSettingsDialog* const q;
    QPointer<CameraSettingsDialogStore> store;
    QPointer<CameraSettingsLicenseWatcher> licenseWatcher;
    QPointer<CameraSettingsReadOnlyWatcher> readOnlyWatcher;
    QPointer<CameraSettingsAnalyticsEnginesWatcher> analyticsEnginesWatcher;
    QPointer<CameraSettingsWearableStateWatcher> wearableStateWatcher;
    QnVirtualCameraResourceList cameras;
    QPointer<QnCamLicenseUsageHelper> licenseUsageHelper;
    QSharedPointer<CameraThumbnailManager> previewManager;
    QPointer<CameraAdvancedSettingsWidget> advancedSettingsWidget;
    QPointer<DeviceAgentSettingsAdapter> deviceAgentSettingsAdaptor;

    Private(CameraSettingsDialog* q): q(q) {}

    void tryReloadAdvancedSettings()
    {
        if (q->currentPage() == int(CameraSettingsTab::advanced))
            advancedSettingsWidget->reloadData();
    }

    void initializeAdvancedSettingsWidget()
    {
        advancedSettingsWidget = new CameraAdvancedSettingsWidget(q->ui->tabWidget);
        installEventHandler(q, QEvent::Show, advancedSettingsWidget,
            [this](QObject* /*watched*/, QEvent* /*event*/)
            {
                advancedSettingsWidget->updateFromResource();
            });

        connect(advancedSettingsWidget, &CameraAdvancedSettingsWidget::hasChangesChanged,
            q, &CameraSettingsDialog::updateButtonsAvailability);
        connect(q->ui->tabWidget, &QTabWidget::currentChanged,
            this, &Private::tryReloadAdvancedSettings);

        const auto updateReadOnlyState =
            [this]() { ::setReadOnly(advancedSettingsWidget, readOnlyWatcher->isReadOnly()); };
        updateReadOnlyState();
        connect(readOnlyWatcher, &CameraSettingsReadOnlyWatcher::readOnlyChanged,
            this, updateReadOnlyState);
    }

    void handleCamerasChanged()
    {
        const bool advancedSettingsAreVisible = cameras.size() == 1;
        if (advancedSettingsAreVisible)
        {
            advancedSettingsWidget->setCamera(cameras.first());
            advancedSettingsWidget->updateFromResource();
            tryReloadAdvancedSettings();
        }
    }

    bool hasChanges() const
    {
        return !cameras.empty()
            && (store->state().hasChanges || advancedSettingsWidget->hasChanges())
            && !store->state().readOnly;
    }

    void applyChanges()
    {
        if (store->state().recording.enabled.valueOr(false))
        {
            if (!licenseUsageHelper->canEnableRecording(cameras))
                store->setRecordingEnabled(false);
        }

        deviceAgentSettingsAdaptor->applySettings();

        store->applyChanges();
        const auto& state = store->state();

        const auto apply =
            [this, &state]
            {
                CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state, cameras);
                if (advancedSettingsWidget->hasChanges())
                    advancedSettingsWidget->submitToResource();
            };

        const auto backout =
            [this, camerasCopy = cameras](bool success)
            {
                if (!success && camerasCopy == cameras)
                    resetChanges();
            };

        qnResourcesChangesManager->saveCamerasBatch(cameras, apply, backout);
    }

    void resetChanges()
    {
        store->loadCameras(cameras);

        handleCamerasChanged();
    }

    void handleAction(ui::action::IDType action)
    {
        switch (action)
        {
            case ui::action::PingAction:
                NX_ASSERT(store);
                q->menu()->trigger(ui::action::PingAction,
                    {Qn::TextRole, store->state().singleCameraProperties.ipAddress});
                break;

            case ui::action::CameraIssuesAction:
            case ui::action::CameraBusinessRulesAction:
            case ui::action::OpenInNewTabAction:
                q->menu()->trigger(action, cameras);
                break;

            case ui::action::UploadWearableCameraFileAction:
            case ui::action::UploadWearableCameraFolderAction:
            case ui::action::CancelWearableCameraUploadsAction:
                NX_ASSERT(cameras.size() == 1
                    && cameras.front()->hasFlags(Qn::wearable_camera));
                if (cameras.size() == 1)
                    q->menu()->trigger(action, cameras.front());
                break;

            case ui::action::CopyRecordingScheduleAction:
                NX_ASSERT(cameras.size() == 1
                    && !cameras.front()->hasFlags(Qn::wearable_camera));
                q->menu()->trigger(action, cameras.front());
                break;

            case ui::action::PreferencesLicensesTabAction:
                q->menu()->trigger(action);
                break;

            case ui::action::ChangeDefaultCameraPasswordAction:
            {
                const auto cameras =
                    QnVirtualCameraResourceList(q->ui->defaultPasswordAlert->cameras().toList());
                const auto parameters = ui::action::Parameters(cameras).withArgument(
                    Qn::ForceShowCamerasList, q->ui->defaultPasswordAlert->useMultipleForm());
                q->menu()->trigger(ui::action::ChangeDefaultCameraPasswordAction, parameters);
                break;
            }

            default:
                NX_ASSERT(false, "Unsupported action request");
        }
    }

    void handleCamerasWithDefaultPasswordChanged()
    {
        const auto defaultPasswordWatcher = q->context()->instance<DefaultPasswordCamerasWatcher>();
        const auto troublesomeCameras = defaultPasswordWatcher->camerasWithDefaultPassword()
            .intersect(cameras.toSet());

        if (!troublesomeCameras.empty())
            q->setCurrentPage(int(CameraSettingsTab::general));

        q->ui->tabWidget->setEnabled(troublesomeCameras.empty());
        q->ui->defaultPasswordAlert->setUseMultipleForm(cameras.size() > 1);
        q->ui->defaultPasswordAlert->setCameras(troublesomeCameras);
    }
};

CameraSettingsDialog::CameraSettingsDialog(QWidget* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent, InitializationMode::lazy),
    ui(new Ui::CameraSettingsDialog()),
    d(new Private(this))
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);
    ui->alertBar->setReservedSpace(false);

    d->store = new CameraSettingsDialogStore(this);

    d->licenseWatcher = new CameraSettingsLicenseWatcher(d->store, this);
    d->readOnlyWatcher = new CameraSettingsReadOnlyWatcher(d->store, this);
    d->analyticsEnginesWatcher = new CameraSettingsAnalyticsEnginesWatcher(d->store, this);
    d->wearableStateWatcher = new CameraSettingsWearableStateWatcher(d->store, this);

    d->licenseUsageHelper = new QnCamLicenseUsageHelper(commonModule(), this);

    d->previewManager.reset(new CameraThumbnailManager());
    d->previewManager->setAutoRotate(true);
    d->previewManager->setThumbnailSize(QSize(0, 0));
    d->previewManager->setAutoRefresh(false);

    d->deviceAgentSettingsAdaptor = new DeviceAgentSettingsAdapter(d->store, this);

    new CameraSettingsGlobalSettingsWatcher(d->store, this);
    new CameraSettingsGlobalPermissionsWatcher(d->store, this);

    auto generalTab = new CameraSettingsGeneralTabWidget(
        d->licenseWatcher->licenseUsageProvider(), d->store, ui->tabWidget);

    connect(generalTab, &CameraSettingsGeneralTabWidget::actionRequested, this,
        [this](ui::action::IDType action) { d->handleAction(action); });

    auto recordingTab = new CameraScheduleWidget(
        d->licenseWatcher->licenseUsageProvider(), d->store, ui->tabWidget);

    connect(recordingTab, &CameraScheduleWidget::actionRequested, this,
        [this](ui::action::IDType action) { d->handleAction(action); });

    addPage(
        int(CameraSettingsTab::general),
        generalTab,
        tr("General"));

    addPage(
        int(CameraSettingsTab::recording),
        recordingTab,
        tr("Recording"));

    addPage(
        int(CameraSettingsTab::io),
        new IoModuleSettingsWidget(d->store, ui->tabWidget),
        tr("I/O Ports"));

    addPage(
        int(CameraSettingsTab::motion),
        new CameraMotionSettingsWidget(d->store, ui->tabWidget),
        tr("Motion"));

    addPage(
        int(CameraSettingsTab::fisheye),
        new CameraFisheyeSettingsWidget(d->previewManager, d->store, ui->tabWidget),
        tr("Fisheye"));

    d->initializeAdvancedSettingsWidget();
    addPage(
        int(CameraSettingsTab::advanced),
        d->advancedSettingsWidget,
        tr("Advanced"));

    addPage(
        int(CameraSettingsTab::web),
        new CameraWebPageWidget(d->store, ui->tabWidget),
        tr("Web Page"));

    addPage(
        int(CameraSettingsTab::analytics),
        new CameraAnalyticsSettingsWidget(
            d->store, qnClientCoreModule->mainQmlEngine(), ui->tabWidget),
        tr("Analytics"));

    addPage(
        int(CameraSettingsTab::expert),
        new CameraExpertSettingsWidget(d->store, ui->tabWidget),
        tr("Expert"));

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(
        selectionWatcher,
        &QnWorkbenchSelectionWatcher::selectionChanged,
        this,
        [this](const QnResourceList& resources)
        {
            if (isHidden())
                return;

            auto cameras = resources.filtered<QnVirtualCameraResource>();
            if (!cameras.isEmpty())
                setCameras(cameras);
        });

    // TODO: #GDM Should we handle current user permission to camera change?
    // TODO: #GDM Shouldn't this logic be incapsulated in the selection watcher?
    connect(
        resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        [this](const QnResourcePtr& resource)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            if (d->cameras.contains(camera))
            {
                auto camerasLeft = d->cameras;
                camerasLeft.removeAll(camera);
                setCameras(camerasLeft, true);
                if (camerasLeft.empty())
                    tryClose(true);
            }
        });

    connect(ui->defaultPasswordAlert, &DefaultPasswordAlertBar::changeDefaultPasswordRequest, this,
        [this]() { d->handleAction(ui::action::ChangeDefaultCameraPasswordAction); });

    const auto defaultPasswordWatcher = context()->instance<DefaultPasswordCamerasWatcher>();
    connect(defaultPasswordWatcher, &DefaultPasswordCamerasWatcher::cameraSetChanged, this,
        [this]() { d->handleCamerasWithDefaultPasswordChanged(); });

    // Make sure we will not handle stateChanged, triggered when creating watchers.
    connect(d->store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraSettingsDialog::updateState);
}

CameraSettingsDialog::~CameraSettingsDialog()
{
}

bool CameraSettingsDialog::tryClose(bool force)
{
    auto result = setCameras({}, force);
    result |= force;
    if (result)
        hide();

    return result;
}

void CameraSettingsDialog::forcedUpdate()
{
}

bool CameraSettingsDialog::setCameras(const QnVirtualCameraResourceList& cameras, bool force)
{
    const bool askConfirmation =
        !force
        && isVisible()
        && d->cameras != cameras
        && (d->hasChanges()
            || d->store->state().analytics.loading);

    if (askConfirmation)
    {
        const auto result = showConfirmationDialog();
        switch (result)
        {
            case QDialogButtonBox::Apply:
                d->applyChanges();
                break;
            case QDialogButtonBox::Discard:
                break;
            default:
                /* Cancel changes. */
                return false;
        }
    }

    const auto singleCamera = cameras.size() == 1 ? cameras.first() : QnVirtualCameraResourcePtr();

    d->cameras = cameras;
    d->resetChanges();
    d->licenseWatcher->setCameras(cameras);
    d->readOnlyWatcher->setCameras(cameras);
    d->wearableStateWatcher->setCameras(cameras);
    d->previewManager->selectCamera(singleCamera);
    d->deviceAgentSettingsAdaptor->setCamera(singleCamera);

    d->handleCamerasWithDefaultPasswordChanged();
    d->handleCamerasChanged();

    return true;
}

void CameraSettingsDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    base_type::buttonBoxClicked(button);
    switch (button)
    {
        case QDialogButtonBox::Ok:
        case QDialogButtonBox::Apply:
            if (d->hasChanges())
                d->applyChanges();
            break;
        case QDialogButtonBox::Cancel:
            d->resetChanges();
            break;
        default:
            break;
    }
}

QDialogButtonBox::StandardButton CameraSettingsDialog::showConfirmationDialog()
{
    if (d->cameras.empty())
        return QDialogButtonBox::Discard;

    const auto extras = QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("Changes to the following %n devices are not saved:", "", d->cameras.size()),
            tr("Changes to the following %n cameras are not saved:", "", d->cameras.size()),
            tr("Changes to the following %n I/O Modules are not saved:", "", d->cameras.size())
        ),
        d->cameras);

    QnMessageBox messageBox(
        QnMessageBoxIcon::Question,
        tr("Apply changes before switching to another camera?"),
        extras,
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply,
        this);

    messageBox.addCustomWidget(new QnResourceListView(d->cameras, &messageBox));
    return QDialogButtonBox::StandardButton(messageBox.exec());
}

void CameraSettingsDialog::updateButtonsAvailability()
{
    if (!buttonBox())
        return;

    const auto& state = d->store->state();

    const auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    const auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    if (okButton)
        okButton->setEnabled(!state.readOnly);

    if (applyButton)
        applyButton->setEnabled(d->hasChanges());
}

void CameraSettingsDialog::updateState()
{
    static const QString kWindowTitlePattern = lit("%1 - %2");

    const auto& state = d->store->state();

    const QString caption = QnCameraDeviceStringSet(
        tr("Device Settings"),
        tr("Devices Settings"),
        tr("Camera Settings"),
        tr("Cameras Settings"),
        tr("I/O Module Settings"),
        tr("I/O Modules Settings")
    ).getString(state.deviceType, state.devicesCount != 1);

    const QString description = state.devicesCount == 1
        ? state.singleCameraProperties.name()
        : QnDeviceDependentStrings::getNumericName(state.deviceType, state.devicesCount);

    setWindowTitle(kWindowTitlePattern.arg(caption).arg(description));

    updateButtonsAvailability();

    // TODO: #vkutin #gdm Ensure correct visibility/enabled state.
    // Legacy code has more complicated conditions.

    using CombinedValue = CameraSettingsDialogState::CombinedValue;

    setPageVisible(int(CameraSettingsTab::motion),
        state.isSingleCamera()
            && state.devicesDescription.isWearable == CombinedValue::None
            && state.devicesDescription.isDtsBased == CombinedValue::None
            && state.devicesDescription.supportsVideo == CombinedValue::All
            && state.devicesDescription.hasMotion == CombinedValue::All);

    setPageVisible(int(CameraSettingsTab::recording), state.supportsSchedule());

    setPageVisible(int(CameraSettingsTab::fisheye),
        state.isSingleCamera()
            && state.singleCameraProperties.hasVideo);

    setPageVisible(int(CameraSettingsTab::io),
        state.isSingleCamera()
            && state.devicesDescription.isWearable == CombinedValue::None
            && state.devicesDescription.isIoModule == CombinedValue::All);

    setPageVisible(int(CameraSettingsTab::web),
        state.isSingleCamera()
            && !state.singleCameraProperties.settingsUrlPath.isEmpty());

    setPageVisible(int(CameraSettingsTab::advanced), state.isSingleCamera());

    setPageVisible(int(CameraSettingsTab::analytics), state.isSingleCamera());

    // Always displaying for single camera as it contains Logical Id setup.
    setPageVisible(int(CameraSettingsTab::expert),
        state.supportsVideoStreamControl() || state.isSingleCamera());
}

} // namespace nx::vms::client::desktop
