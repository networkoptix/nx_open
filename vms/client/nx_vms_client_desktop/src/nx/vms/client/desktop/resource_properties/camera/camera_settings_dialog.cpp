// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_dialog.h"
#include "ui_camera_settings_dialog.h"

#include <chrono>
#include <memory>

#include <QtCore/QSharedPointer>
#include <QtWidgets/QPushButton>

#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/ptz/remote_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/utils/qset.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/thumbnails/live_camera_thumbnail.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "../fisheye/fisheye_preview_controller.h"
#include "camera_advanced_parameters_manifest_manager.h"
#include "camera_advanced_params_widget.h"
#include "camera_settings_tab.h"
#include "flux/camera_settings_dialog_state.h"
#include "flux/camera_settings_dialog_store.h"
#include "utils/camera_settings_dialog_state_conversion_functions.h"
#include "utils/device_agent_settings_adapter.h"
#include "utils/license_usage_provider.h"
#include "watchers/camera_settings_advanced_manifest_watcher.h"
#include "watchers/camera_settings_analytics_engines_watcher.h"
#include "watchers/camera_settings_global_permissions_watcher.h"
#include "watchers/camera_settings_global_settings_watcher.h"
#include "watchers/camera_settings_license_watcher.h"
#include "watchers/camera_settings_ptz_capabilities_watcher.h"
#include "watchers/camera_settings_readonly_watcher.h"
#include "watchers/camera_settings_remote_changes_watcher.h"
#include "watchers/virtual_camera_settings_state_watcher.h"
#include "widgets/camera_analytics_settings_widget.h"
#include "widgets/camera_dewarping_settings_widget.h"
#include "widgets/camera_expert_settings_widget.h"
#include "widgets/camera_motion_settings_widget.h"
#include "widgets/camera_schedule_widget.h"
#include "widgets/camera_settings_general_tab_widget.h"
#include "widgets/camera_web_page_widget.h"
#include "widgets/io_module_settings_widget.h"

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;

static constexpr auto kPreviewReloadInterval = 5s;

} // namespace

struct CameraSettingsDialog::Private: public QObject
{
    CameraSettingsDialog* const q;
    QPointer<CameraSettingsDialogStore> store;
    QPointer<CameraSettingsLicenseWatcher> licenseWatcher;
    QPointer<CameraSettingsReadOnlyWatcher> readOnlyWatcher;
    QPointer<CameraSettingsAnalyticsEnginesWatcher> analyticsEnginesWatcher;
    QPointer<VirtualCameraSettingsStateWatcher> virtualCameraStateWatcher;
    QPointer<CameraSettingsRemoteChangesWatcher> cameraPropertyWatcher;
    QPointer<CameraSettingsPtzCapabilitiesWatcher> cameraPtzCapabilitiesWatcher;
    QnVirtualCameraResourceList cameras;
    QPointer<nx::vms::license::CamLicenseUsageHelper> licenseUsageHelper;
    QPointer<CameraAdvancedParamsWidget> advancedSettingsWidget;
    QPointer<DeviceAgentSettingsAdapter> deviceAgentSettingsAdapter;
    QPointer<FisheyePreviewController> fisheyePreviewController;
    QPointer<QPushButton> fixupScheduleButton;
    std::unique_ptr<CameraAdvancedParametersManifestManager> advancedParametersManifestManager;
    std::unique_ptr<CameraSettingsAdvancedManifestWatcher> advancedParametersManifestWatcher;

    const QSharedPointer<LiveCameraThumbnail> cameraPreview{new LiveCameraThumbnail()};

    Private(CameraSettingsDialog* q): q(q)
    {
        const auto timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &Private::updatePreviewIfNeeded);
        timer->start(kPreviewReloadInterval);

        cameraPreview->setMaximumSize(LiveCameraThumbnail::kUnlimitedSize);
    }

    void initializeAdvancedSettingsWidget()
    {
        advancedParametersManifestManager.reset(new CameraAdvancedParametersManifestManager());
        advancedParametersManifestWatcher.reset(new CameraSettingsAdvancedManifestWatcher(
            advancedParametersManifestManager.get(),
            appContext()->currentSystemContext()->serverRuntimeEventConnector(),
            store));

        advancedSettingsWidget = new CameraAdvancedParamsWidget(store, q->ui->tabWidget);
        connect(advancedSettingsWidget, &CameraAdvancedParamsWidget::hasChangesChanged,
            this, [this]() { q->updateButtonsAvailability(store->state()); });
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

        // Actual for the single-camera mode only.
        if (store->state().recording.enabled.valueOr(false))
        {
            for (const auto& camera: cameras)
            {
                const auto& server = camera->getParentServer();

                if (server.isNull())
                    continue;

                if (server->metadataStorageId().isNull())
                {
                    // We need to choose analytics storage locations.
                    q->workbench()->context()->menu()->triggerIfPossible(
                        ui::action::ConfirmAnalyticsStorageAction);
                            //< TODO: #spanasenko Specify the server ID?
                }
            }
        }

        deviceAgentSettingsAdapter->applySettings();

        const auto& state = store->state();

        const auto apply =
            [this, &state]
            {
                CameraSettingsDialogStateConversionFunctions::applyStateToCameras(state, cameras);
                if (advancedSettingsWidget->hasChanges())
                    advancedSettingsWidget->saveValues();

                for (const auto& camera: cameras)
                    camera->updatePreferredServerId();

                resetChanges();
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
        const auto singleCamera = cameras.size() == 1
            ? cameras.first()
            : QnVirtualCameraResourcePtr();

        store->loadCameras(
            cameras,
            deviceAgentSettingsAdapter,
            analyticsEnginesWatcher,
            advancedParametersManifestManager.get());
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

            case ui::action::UploadVirtualCameraFileAction:
            case ui::action::UploadVirtualCameraFolderAction:
            case ui::action::CancelVirtualCameraUploadsAction:
                NX_ASSERT(cameras.size() == 1
                    && cameras.front()->hasFlags(Qn::virtual_camera));
                if (cameras.size() == 1)
                    q->menu()->trigger(action, cameras.front());
                break;

            case ui::action::CopyRecordingScheduleAction:
            {
                if (NX_ASSERT(store && store->state().recording.schedule.hasValue()))
                    q->menu()->trigger(action);
                break;
            }

            case ui::action::PreferencesLicensesTabAction:
                q->menu()->trigger(action);
                break;

            case ui::action::ChangeDefaultCameraPasswordAction:
            {
                const auto cameras =
                    QnVirtualCameraResourceList(q->ui->defaultPasswordAlert->cameras().values());
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
        const auto defaultPasswordWatcher =
            q->context()->findInstance<DefaultPasswordCamerasWatcher>();
        const auto troublesomeCameras = defaultPasswordWatcher->camerasWithDefaultPassword()
            .intersect(nx::utils::toQSet(cameras));

        if (!troublesomeCameras.empty())
            q->setCurrentPage(int(CameraSettingsTab::general));

        q->ui->tabWidget->setEnabled(troublesomeCameras.empty());
        q->ui->defaultPasswordAlert->setUseMultipleForm(cameras.size() > 1);
        q->ui->defaultPasswordAlert->setCameras(troublesomeCameras);
    }

    void updatePreviewIfNeeded()
    {
        if (q->isVisible() && q->currentPage() == int(CameraSettingsTab::dewarping))
            cameraPreview->update();
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
    d->store = new CameraSettingsDialogStore(this);

    d->licenseWatcher = new CameraSettingsLicenseWatcher(d->store, this);
    d->readOnlyWatcher = new CameraSettingsReadOnlyWatcher(d->store, this);
    d->analyticsEnginesWatcher = new CameraSettingsAnalyticsEnginesWatcher(d->store, this);
    d->virtualCameraStateWatcher = new VirtualCameraSettingsStateWatcher(d->store, this);
    d->cameraPropertyWatcher = new CameraSettingsRemoteChangesWatcher(d->store, this);
    d->cameraPtzCapabilitiesWatcher = new CameraSettingsPtzCapabilitiesWatcher(d->store, this);

    d->licenseUsageHelper = new nx::vms::license::CamLicenseUsageHelper(systemContext(), this);

    d->deviceAgentSettingsAdapter = new DeviceAgentSettingsAdapter(d->store, context(), this);

    d->fisheyePreviewController = new FisheyePreviewController(this);

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
        int(CameraSettingsTab::dewarping),
        new CameraDewarpingSettingsWidget(
            d->store, d->cameraPreview, qnClientCoreModule->mainQmlEngine(), ui->tabWidget),
        tr("Dewarping"));

    d->initializeAdvancedSettingsWidget();
    addPage(
        int(CameraSettingsTab::advanced),
        d->advancedSettingsWidget,
        tr("Advanced"));

    auto cameraWebPage = new CameraWebPageWidget(d->store, ui->tabWidget);
    connect(this, &QDialog::finished, cameraWebPage, &CameraWebPageWidget::cleanup);
    addPage(
        int(CameraSettingsTab::web),
        cameraWebPage,
        tr("Web Page"));

    addPage(
        int(CameraSettingsTab::analytics),
        new CameraAnalyticsSettingsWidget(
            d->store, qnClientCoreModule->mainQmlEngine(), ui->tabWidget),
        tr("Plugins"));

    addPage(
        int(CameraSettingsTab::expert),
        new CameraExpertSettingsWidget(d->store, ui->tabWidget),
        tr("Expert"));

    autoResizePagesToContents(ui->tabWidget, QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred),
        true);

    d->fixupScheduleButton = new QPushButton(ui->scheduleAlertBar);
    d->fixupScheduleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    d->fixupScheduleButton->setText(tr("Change invalid schedule to \"Record Always\""));
    ui->scheduleAlertBar->verticalLayout()->addWidget(d->fixupScheduleButton.data());
    connect(d->fixupScheduleButton.data(), &QPushButton::clicked, this,
        [this]()
        {
            d->store->fixupSchedule();
        });

    // TODO: #sivanov Should we handle current user permission to camera change?
    // TODO: #sivanov Shouldn't this logic be encapsulated in the selection watcher?
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

    const auto defaultPasswordWatcher = context()->findInstance<DefaultPasswordCamerasWatcher>();
    connect(defaultPasswordWatcher, &DefaultPasswordCamerasWatcher::cameraSetChanged, this,
        [this]() { d->handleCamerasWithDefaultPasswordChanged(); });

    // Make sure we will not handle stateChanged, triggered when creating watchers.
    connect(d->store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraSettingsDialog::loadState);

    connect(d->store,
        &CameraSettingsDialogStore::stateChanged,
        d->fisheyePreviewController,
        [this](const CameraSettingsDialogState& state)
        {
            const auto singleCamera = d->cameras.size() == 1
                ? d->cameras.first()
                : QnVirtualCameraResourcePtr();

            d->fisheyePreviewController->preview(
                singleCamera,
                state.singleCameraSettings.dewarpingParams());
        });

    connect(ui->tabWidget, &QTabWidget::currentChanged, this,
        [this]
        {
            d->store->setSelectedTab((CameraSettingsTab)currentPage());
        });

    connect(ui->tabWidget, &QTabWidget::currentChanged, d.get(), &Private::updatePreviewIfNeeded);

    setHelpTopic(this, Qn::CameraSettings_Help);
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
    // Ignore click on the same camera.
    if (!force && d->cameras == cameras)
        return true;

    const bool askConfirmation =
        !force
        && isVisible()
        && !(d->cameras == cameras)
        && d->hasChanges();

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

    // All correctly implemented watchers are going before 'resetChanges' as their 'setCamera'
    // method is clean and does not send anything to the store.
    d->analyticsEnginesWatcher->setCamera(singleCamera);
    d->deviceAgentSettingsAdapter->setCamera(singleCamera);
    d->advancedParametersManifestWatcher->selectCamera(singleCamera);

    d->advancedSettingsWidget->setSelectedServer({});
    d->advancedSettingsWidget->setPtzInterface({});
    if (singleCamera)
    {
        if (const auto server = singleCamera->getParentServer())
            d->advancedSettingsWidget->setSelectedServer(server->getId());
        d->advancedSettingsWidget->setPtzInterface(
            std::make_unique<QnRemotePtzController>(singleCamera));
    }

    d->resetChanges();

    // These watchers can modify store during 'setCamera' call.
    d->licenseWatcher->setCameras(cameras);
    d->readOnlyWatcher->setCameras(cameras);
    d->virtualCameraStateWatcher->setCameras(cameras);
    d->cameraPropertyWatcher->setCameras(cameras);

    d->cameraPreview->setResource(singleCamera);
    d->cameraPreview->update(/*forceRefresh*/ true);

    const auto camerasSet = nx::utils::toQSet(cameras);
    d->cameraPtzCapabilitiesWatcher->setCameras(camerasSet);

    d->handleCamerasWithDefaultPasswordChanged();

    return true;
}

const CameraSettingsDialogState& CameraSettingsDialog::state() const
{
    return d->store->state();
}

void CameraSettingsDialog::done(int result)
{
    base_type::done(result);
    setCameras({}, /*force*/ true);
}

void CameraSettingsDialog::showEvent(QShowEvent* event)
{
    // Load state into tabs in case the state was changed outside current client process.
    d->resetChanges();
    d->deviceAgentSettingsAdapter->refreshSettings();

    base_type::showEvent(event);
    d->cameraPreview->update(/*forceRefresh*/ true);
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

void CameraSettingsDialog::updateButtonsAvailability(const CameraSettingsDialogState& state)
{
    if (!buttonBox())
        return;

    const auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    const auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    if (okButton)
        okButton->setEnabled(!state.readOnly);

    if (applyButton)
        applyButton->setEnabled(d->hasChanges());
}

void CameraSettingsDialog::updateScheduleAlert(const CameraSettingsDialogState& state)
{
    if (state.scheduleAlerts == CameraSettingsDialogState::ScheduleAlerts())
    {
        ui->scheduleAlertBar->setText(QString());
        return;
    }

    using ScheduleAlert = CameraSettingsDialogState::ScheduleAlert;

    QString scheduleAlert = tr("Some cells of the recording schedule are set to an invalid "
        "recording mode: %1. During the hours set to an invalid recording mode camera will record "
        "in \"Record Always\" mode.");

    QStringList unsupportedOptions;
    if (state.scheduleAlerts.testFlag(ScheduleAlert::noMotion))
        unsupportedOptions << tr("Motion Only");
    if (state.scheduleAlerts.testFlag(ScheduleAlert::noObjects))
        unsupportedOptions << tr("Objects Only");
    if (state.scheduleAlerts.testFlag(ScheduleAlert::noBoth))
        unsupportedOptions << tr("Motion & Objects Only");
    if (state.scheduleAlerts.testFlag(ScheduleAlert::noMotionLowRes))
        unsupportedOptions << tr("Motion + Lo-Res");
    if (state.scheduleAlerts.testFlag(ScheduleAlert::noObjectsLowRes))
        unsupportedOptions << tr("Objects + Lo-Res");
    if (state.scheduleAlerts.testFlag(ScheduleAlert::noBothLowRes))
        unsupportedOptions << tr("Motion & Objects + Lo-Res");

    ui->scheduleAlertBar->setText(scheduleAlert.arg(unsupportedOptions.join(", ")));

    d->fixupScheduleButton->setVisible(state.isSingleCamera());
}

void CameraSettingsDialog::loadState(const CameraSettingsDialogState& state)
{
    static const QString kWindowTitlePattern = lit("%1 - %2");

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

    updateButtonsAvailability(state);
    updateScheduleAlert(state);

    static const int kLastTabKey = (int)CameraSettingsTab::expert;

    for (int key = 0; key <= kLastTabKey; ++key)
        setPageVisible(key, state.isPageVisible((CameraSettingsTab)key));

    setCurrentPage((int)state.selectedTab);
}

} // namespace nx::vms::client::desktop
