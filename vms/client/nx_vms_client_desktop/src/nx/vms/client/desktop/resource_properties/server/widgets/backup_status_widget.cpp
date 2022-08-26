// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_status_widget.h"

#include "ui_backup_status_widget.h"

#include <api/runtime_info_manager.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/time/formatter.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

using namespace std::chrono;

namespace {

nx::vms::api::BackupPosition backupPositionFromCurrentTime()
{
    const auto currentTimePointMs = system_clock::time_point(qnSyncTime->value());

    nx::vms::api::BackupPosition result;
    result.positionHighMs = currentTimePointMs;
    result.positionLowMs = currentTimePointMs;
    result.bookmarkStartPositionMs = currentTimePointMs;

    return result;
}

QnVirtualCameraResourceList backupEnabledCameras(const QnMediaServerResourcePtr& server)
{
    if (server.isNull())
        return {};

    const auto resourcePool = server->resourcePool();
    return resourcePool->getAllCameras(server->getId(), /*ignoreDesktopCameras*/ true)
        .filtered<QnVirtualCameraResource>(
            [](const auto& camera)
            {
                return camera->isBackupEnabled()
                    && nx::vms::client::desktop::backup_settings_view::isBackupSupported(camera)
                    && !camera->hasFlags(Qn::ResourceFlag::removed);
            });
}

bool hasBackupStorageIssue(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    const auto runtimeInfoManager = qnClientCoreModule->commonModule()->runtimeInfoManager();
    if (runtimeInfoManager->hasItem(server->getId()))
    {
        const auto runtimeFlags = runtimeInfoManager->item(server->getId()).data.flags;
        return runtimeFlags.testFlag(nx::vms::api::RuntimeFlag::noBackupStorages);
    }

    return false;
}

} // namespace

namespace nx::vms::client::desktop {

BackupStatusWidget::BackupStatusWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::BackupStatusWidget())
{
    ui->setupUi(this);

    const auto lightAccentColor = colorTheme()->color("light10");
    setPaletteColor(ui->backupNotConfiguredStatusLabel, QPalette::WindowText, lightAccentColor);
    setPaletteColor(ui->backupCamerasCountLabel, QPalette::WindowText, lightAccentColor);
    setWarningStyle(ui->storageIssueLabel);

    ui->refreshStatusButton1->setIcon(qnSkin->icon(lit("text_buttons/refresh.png")));
    ui->refreshStatusButton2->setIcon(qnSkin->icon(lit("text_buttons/refresh.png")));
    connect(ui->refreshStatusButton1, &QPushButton::clicked,
        this, &BackupStatusWidget::updateBackupStatus);
    connect(ui->refreshStatusButton2, &QPushButton::clicked,
        this, &BackupStatusWidget::updateBackupStatus);

    setupSkipBackupButton();

    ui->skipQueueHintButton->addHintLine(tr(
        "Backup process will ignore existing footage. Only further recording will be backed up."));

    ui->skipQueueHintButton->addHintLine(tr(
        "Applies only to the cameras connected to current server."));

    const auto runtimeInfoManager = qnClientCoreModule->commonModule()->runtimeInfoManager();
    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo& runtimeInfo)
        {
            if (!m_server || runtimeInfo.uuid != m_server->getId())
                return;

            const auto hasStorageIssue =
                runtimeInfo.data.flags.testFlag(nx::vms::api::RuntimeFlag::noBackupStorages);

            if (m_hasBackupStorageIssue == hasStorageIssue)
                return;

            m_hasBackupStorageIssue = hasStorageIssue;
            updateBackupStatus();
        });
}

BackupStatusWidget::~BackupStatusWidget()
{
}

void BackupStatusWidget::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_server = server;
    m_hasBackupStorageIssue = hasBackupStorageIssue(m_server);
    updateBackupStatus();
}

void BackupStatusWidget::setupSkipBackupButton()
{
    connect(ui->skipQueuePushButton, &QPushButton::clicked, this,
        [this]
        {
            const auto messageBox = std::make_unique<QnMessageBox>(
                QnMessageBox::Icon::Question,
                tr("Skip backup for existing footage?"),
                tr("This action cannot be undone."),
                QDialogButtonBox::StandardButton::Cancel,
                QDialogButtonBox::StandardButton::NoButton,
                window());

            const auto skipButton = messageBox->addCustomButton(
                QnMessageBoxCustomButton::Skip,
                QDialogButtonBox::ButtonRole::YesRole,
                Qn::ButtonAccent::Warning);

            messageBox->exec();
            if (messageBox->clickedButton() != skipButton)
                return;

            const auto cameras = backupEnabledCameras(m_server);
            const auto userBackupPosition = backupPositionFromCurrentTime();
            const auto mutex = std::make_shared<nx::Mutex>();
            const auto requestHandles = std::make_shared<std::set<rest::Handle>>();

            ui->skipQueuePushButton->setEnabled(false);

            const auto callback =
                [this, mutex, requestHandles]
                (bool success, rest::Handle requestId, nx::vms::api::BackupPosition result)
                {
                    NX_MUTEX_LOCKER lock(mutex.get());
                    if (auto count = requestHandles->erase(requestId);
                        count > 0 && requestHandles->empty())
                    {
                        QMetaObject::invokeMethod(
                            ui->skipQueuePushButton,
                            "setEnabled",
                            Qt::QueuedConnection,
                            Q_ARG(bool, true));

                        QMetaObject::invokeMethod(this, "updateBackupStatus", Qt::QueuedConnection);
                    }
                };

            if (!NX_ASSERT(connection()))
                return;

            NX_MUTEX_LOCKER lock(mutex.get());
            requestHandles->insert(connectedServerApi()->setBackupPositionsAsync(
                m_server->getId(), userBackupPosition, callback));
        });
}

void BackupStatusWidget::updateBackupStatus()
{
    m_interruptBackupQueueSizeCalculation = true;

    if (m_server.isNull() || !NX_ASSERT(connection()))
    {
        ui->stackedWidget->setCurrentWidget(ui->backupNotConfiguredPage);
        return;
    }

    const auto cameras = backupEnabledCameras(m_server);
    if (cameras.empty())
    {
        ui->stackedWidget->setCurrentWidget(ui->backupNotConfiguredPage);
        return;
    }

    const auto backupCamerasCountText =
        QnDeviceDependentStrings::getDefaultNameFromSet(m_server->resourcePool(),
            tr("Backup is enabled for %n devices", "", cameras.count()),
            tr("Backup is enabled for %n cameras", "", cameras.count()));

    ui->backupCamerasCountLabel->setText(backupCamerasCountText);

    if (m_hasBackupStorageIssue)
    {
        ui->stackedWidget->setCurrentWidget(ui->backupConfiguredPage);
        ui->descriptionStackedWidget->setCurrentWidget(ui->storageIssuePage);
        ui->skipQueueWidget->setHidden(true);
        return;
    }

    ui->stackedWidget->setCurrentWidget(ui->preloaderPage);

    const auto mutex = std::make_shared<nx::Mutex>();
    const auto backupPositionRequestHandles = std::make_shared<std::set<rest::Handle>>();

    const auto currentTimePoint = milliseconds(qnSyncTime->currentMSecsSinceEpoch());
    const auto backupTimePoint = std::make_shared<milliseconds>(currentTimePoint);

    const auto actualPositionCallback = nx::utils::guarded(this,
        [this, mutex, server = m_server, backupPositionRequestHandles,
            currentTimePoint, backupTimePoint]
        (bool success, rest::Handle requestId, nx::vms::api::BackupPositionEx actualPosition)
        {
            NX_MUTEX_LOCKER lock(mutex.get());
            if (auto count = backupPositionRequestHandles->erase(requestId); count > 0 && success)
            {
                const auto deviceBackupTimePoint = currentTimePoint -
                    std::max(actualPosition.toBackupHighMs, actualPosition.toBackupLowMs);
                *backupTimePoint = std::min(*backupTimePoint, deviceBackupTimePoint);

                if (backupPositionRequestHandles->empty())
                {
                    executeLater(
                        [this, server, timePoint = *backupTimePoint]
                        {
                            onBackupTimePointCalculated(server, timePoint);
                        }, this);
                }
            }
        });

    m_interruptBackupQueueSizeCalculation = false;
    for (const auto& camera: cameras)
    {
        NX_MUTEX_LOCKER lock(mutex.get());
        backupPositionRequestHandles->insert(m_server->restConnection()->backupPositionAsync(
            m_server->getId(),
            camera->getId(),
            actualPositionCallback));
    }
}

void BackupStatusWidget::onBackupTimePointCalculated(
    const QnMediaServerResourcePtr server,
    const milliseconds backupTimePoint)
{
    namespace html = nx::vms::common::html;
    namespace time = nx::vms::time;

    // Maximum backup position lag at which the presence of non-backed up data won't be reported.
    static constexpr auto kAllBackedUpThreshold = 30s;

    if (server != m_server || m_interruptBackupQueueSizeCalculation)
        return;

    const auto currentTimePoint = milliseconds(qnSyncTime->currentMSecsSinceEpoch());

    ui->stackedWidget->setCurrentWidget(ui->backupConfiguredPage);
    if (currentTimePoint - backupTimePoint < kAllBackedUpThreshold)
    {
        ui->descriptionStackedWidget->setCurrentWidget(ui->allBackedUpPage);
        ui->skipQueueWidget->setHidden(true);
    }
    else
    {
        ui->descriptionStackedWidget->setCurrentWidget(ui->backupQueuePage);
        ui->skipQueueWidget->setHidden(false);
    }

    const auto backupTimePointLabelText =
        tr("Footage from these cameras is backed up through to %1 %2",
            "%1 and %2 will be replaced respectively by the date and time in the system format.");

    const auto formatDateTime =
        [](const QString& text)
        {
            static const auto lightAccentColor = colorTheme()->color("light10");
            auto result = html::colored(text, lightAccentColor);
            result.prepend(QChar::Nbsp); //< Extra spacing for better visual perception;
            return result;
        };

    const auto backupDateTimePoint = QDateTime::fromMSecsSinceEpoch(backupTimePoint.count());
    ui->backupTimePointLabel->setText(backupTimePointLabelText
        .arg(formatDateTime(time::toString(backupDateTimePoint.date())))
        .arg(formatDateTime(time::toString(backupDateTimePoint.time()))));
}

void BackupStatusWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    updateBackupStatus();
}

} // namespace nx::vms::client::desktop
