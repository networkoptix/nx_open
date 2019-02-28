#include "connect_to_current_system_tool.h"

#include <chrono>

#include <nx/utils/log/log.h>

#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/fake_media_server.h>
#include <common/common_module.h>

#include <network/system_helpers.h>

#include <utils/merge_systems_tool.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
//#include <update/media_server_update_tool.h>

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static const int kEmptyProgress = 0;
static const int kUpdateProgress = 50;
static const milliseconds kWaitTimeout = 2min;

QnFakeMediaServerResourcePtr incompatibleServerForOriginalId(
    const QnUuid& id, const QnResourcePool* resourcePool)
{
    for (const auto& server: resourcePool->getIncompatibleServers())
    {
        if (const auto& fakeServer = server.dynamicCast<QnFakeMediaServerResource>())
        {
            if (fakeServer->getOriginalGuid() == id)
                return fakeServer;
        }
    }

    return {};
}

} // namespace

namespace nx::vms::client::desktop {

ConnectToCurrentSystemTool::ConnectToCurrentSystemTool(QObject* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent)
{
}

ConnectToCurrentSystemTool::~ConnectToCurrentSystemTool() {}

bool ConnectToCurrentSystemTool::tryClose(bool /*force*/)
{
    cancel();
    return true;
}

void ConnectToCurrentSystemTool::forcedUpdate()
{
}

void ConnectToCurrentSystemTool::start(const QnUuid& targetId, const QString& password)
{
    NX_ASSERT(!targetId.isNull());
    if (targetId.isNull())
        return;

    auto server = resourcePool()->getIncompatibleServerById(targetId, true);
    NX_ASSERT(server);
    if (!server)
        return;

    m_targetId = targetId;
    m_adminPassword = password;
    m_originalTargetId = server->getOriginalGuid();
    auto serverInformation = server->getModuleInformation();
    m_serverName = server->getName();
    m_serverVersion = serverInformation.version;
    m_wasIncompatible = serverInformation.protoVersion != QnAppInfo::ec2ProtoVersion();

    NX_INFO(this, lm("Start connecting server %1 (id=%2, url=%3").args(
        m_serverName, m_originalTargetId, server->getApiUrl()));

    emit progressChanged(kUpdateProgress);
    mergeServer();
}

::utils::MergeSystemsStatus::Value ConnectToCurrentSystemTool::mergeError() const
{
    return m_mergeError;
}

QString ConnectToCurrentSystemTool::mergeErrorMessage() const
{
    return m_mergeErrorMessage;
}

QnUuid ConnectToCurrentSystemTool::getTargetId() const
{
    return m_targetId;
}

QnUuid ConnectToCurrentSystemTool::getOriginalId() const
{
    return m_originalTargetId;
}

bool ConnectToCurrentSystemTool::wasServerIncompatible() const
{
    return m_wasIncompatible;
}

nx::utils::SoftwareVersion ConnectToCurrentSystemTool::getServerVersion() const
{
    return m_serverVersion;
}

QString ConnectToCurrentSystemTool::getServerName() const
{
    return m_serverName;
}

void ConnectToCurrentSystemTool::cancel()
{
    if (m_mergeTool)
        m_mergeTool->deleteLater();

    /* dkargin: I want to keep this code for some time.
    if (m_updateTool)
    {
        m_updateTool->cancelUpdate();
        m_updateTool->deleteLater();
    }*/

    if (m_mergeTool/* || m_updateTool*/)
        finish(Canceled);
}

void ConnectToCurrentSystemTool::finish(ErrorCode errorCode)
{
    using nx::utils::log::Level;

    NX_UTILS_LOG(errorCode == NoError ? Level::info : Level::error,
        this, lm("Finished: %1").arg(errorCode));

    emit finished(errorCode);
}

void ConnectToCurrentSystemTool::mergeServer()
{
    NX_ASSERT(!m_mergeTool);

    m_mergeError = MergeSystemsStatusValue::ok;
    m_mergeErrorMessage.clear();

    const auto server = incompatibleServerForOriginalId(m_originalTargetId, resourcePool());
    if (!server)
    {
        const auto compatibleServer =
            resourcePool()->getResourceById<QnMediaServerResource>(m_originalTargetId);
        if (compatibleServer && compatibleServer->getStatus() == Qn::Online)
        {
            finish(NoError);
            return;
        }

        m_mergeError = MergeSystemsStatusValue::unknownError;
        finish(MergeFailed);
        return;
    }

    auto serverInformation = server->getModuleInformation();

    const auto ecServer = commonModule()->currentServer();
    if (!ecServer)
        return;

    QAuthenticator auth;
    auth.setUser(helpers::kFactorySystemUser);
    auth.setPassword(m_adminPassword);

    NX_INFO(this, "Start server configuration.");

    emit progressChanged(0);
    emit stateChanged(tr("Configuring Server"));

    m_mergeTool = new QnMergeSystemsTool(this);

    connect(m_mergeTool, &QnMergeSystemsTool::mergeFinished, this,
        [this](
            MergeSystemsStatusValue mergeStatus,
            const nx::vms::api::ModuleInformation& moduleInformation)
        {
            if (mergeStatus != MergeSystemsStatusValue::ok)
            {
                delete m_mergeTool;
                m_mergeError = mergeStatus;
                m_mergeErrorMessage =
                    ::utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation);

                NX_ERROR(this, lm("Server configuration failed: %1.").arg(m_mergeErrorMessage));

                finish(MergeFailed);
                return;
            }

            NX_INFO(this, "Server configuration finished.");

            waitServer();
        });

    m_mergeTool->configureIncompatibleServer(ecServer, server->getApiUrl(), auth);
}

void ConnectToCurrentSystemTool::waitServer()
{
    NX_ASSERT(m_mergeTool);

    auto finishMerge = [this](ErrorCode result)
        {
            resourcePool()->disconnect(this);
            delete m_mergeTool;
            finish(result);
        };

    if (resourcePool()->getIncompatibleServerById(m_targetId, true))
    {
        finishMerge(NoError);
        return;
    }

    auto handleResourceChanged = [this, finishMerge](const QnResourcePtr& resource)
        {
            if (resource->getId() == m_originalTargetId && resource->getStatus() != Qn::Offline)
                finishMerge(NoError);
        };

    NX_INFO(this, "Start waiting server.");

    // Receiver object is m_mergeTool.
    // This helps us to break the connection when m_mergeTool is deleted in the cancel() method.
    connect(resourcePool(), &QnResourcePool::resourceAdded, m_mergeTool, handleResourceChanged);
    connect(resourcePool(), &QnResourcePool::statusChanged, m_mergeTool, handleResourceChanged);

    executeDelayedParented(
        [finishMerge]()
        {
            finishMerge(MergeFailed);
        }, kWaitTimeout.count(), m_mergeTool);

    emit progressChanged(kUpdateProgress);
}

void ConnectToCurrentSystemTool::updateServer()
{
    // This will be disabled until new update system is used for the merge.
    // dkargin: I would like to keep this code for some time.
#ifdef OLD_UPDATE_FOR_REFERENCE
    NX_ASSERT(!m_updateTool);

    emit stateChanged(tr("Updating Server"));
    emit progressChanged(kEmptyProgress);

    m_updateTool = new QnMediaServerUpdateTool(this);

    connect(m_updateTool, &QnMediaServerUpdateTool::updateFinished, this,
        [this](const QnUpdateResult& result)
        {
            m_updateTool->deleteLater();
            m_updateTool.clear();

            if (const auto fakeServer = resourcePool()->getIncompatibleServerById(m_targetId)
                .dynamicCast<QnFakeMediaServerResource>())
            {
                fakeServer->setAuthenticator(QAuthenticator());
            }

            m_updateResult = result;

            if (result.result != QnUpdateResult::Successful)
            {
                NX_ERROR(this, lm("Server update failed: %1.").arg(result.result));
                finish(UpdateFailed);
                return;
            }

            NX_INFO(this, "Server update finished.");

            emit progressChanged(kUpdateProgress);
            mergeServer();
        });

    connect(m_updateTool, &QnMediaServerUpdateTool::stageProgressChanged, this,
        [this](QnFullUpdateStage stage, int progress)
        {
            const int updateProgress =
                ((int) stage * 100 + progress) / (int) QnFullUpdateStage::Count;
            emit progressChanged(updateProgress * kUpdateProgress / 100);
        });

    auto targetVersion = commonModule()->engineVersion();
    if (const auto ecServer = commonModule()->currentServer())
        targetVersion = ecServer->getVersion();

    if (auto fakeServer = server.dynamicCast<QnFakeMediaServerResource>())
    {
        QAuthenticator authenticator;
        authenticator.setUser(helpers::kFactorySystemUser);
        authenticator.setPassword(m_adminPassword);
        fakeServer->setAuthenticator(authenticator);
    }

    m_updateTool->setTargets({m_targetId});
    m_updateTool->startUpdate(targetVersion);
#endif
}

} // namespace nx::vms::client::desktop
