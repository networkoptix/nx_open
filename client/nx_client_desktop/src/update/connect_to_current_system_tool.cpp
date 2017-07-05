#include "connect_to_current_system_tool.h"

#include <chrono>

#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/fake_media_server.h>
#include <common/common_module.h>

#include <network/system_helpers.h>

#include <utils/merge_systems_tool.h>
#include <utils/common/software_version.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>

#include <update/media_server_update_tool.h>

namespace {

    using namespace std::chrono;

    static const int kEmptyProgress = 0;
    static const int kUpdateProgress = 50;
    static const int kCompleteProgress = 100;
    static const milliseconds kWaitTimeout = seconds(2);

} // namespace

QnConnectToCurrentSystemTool::QnConnectToCurrentSystemTool(QObject* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_mergeTool(nullptr),
    m_updateTool(nullptr)
{
}

QnConnectToCurrentSystemTool::~QnConnectToCurrentSystemTool() {}

bool QnConnectToCurrentSystemTool::tryClose(bool /*force*/)
{
    cancel();
    return true;
}

void QnConnectToCurrentSystemTool::forcedUpdate()
{
}

void QnConnectToCurrentSystemTool::start(const QnUuid& targetId, const QString& password)
{
    NX_ASSERT(!targetId.isNull());
    if (targetId.isNull())
        return;

    auto server = resourcePool()->getIncompatibleResourceById(targetId, true);
    NX_ASSERT(server);
    if (!server)
        return;

    m_targetId = targetId;
    m_adminPassword = password;
    m_originalTargetId = server->getOriginalGuid();

    if (auto fakeServer = server.dynamicCast<QnFakeMediaServerResource>())
    {
        QAuthenticator authenticator;
        authenticator.setUser(helpers::kFactorySystemUser);
        authenticator.setPassword(m_adminPassword);
        fakeServer->setAuthenticator(authenticator);
    }

    updateServer();
}

utils::MergeSystemsStatus::Value QnConnectToCurrentSystemTool::mergeError() const
{
    return m_mergeError;
}

QString QnConnectToCurrentSystemTool::mergeErrorMessage() const
{
    return m_mergeErrorMessage;
}

QnUpdateResult QnConnectToCurrentSystemTool::updateResult() const
{
    return m_updateResult;
}

void QnConnectToCurrentSystemTool::cancel()
{
    if (m_mergeTool)
        m_mergeTool->deleteLater();

    if (m_updateTool)
    {
        m_updateTool->cancelUpdate();
        m_updateTool->deleteLater();
    }

    if (m_mergeTool || m_updateTool)
        emit finished(Canceled);
}

void QnConnectToCurrentSystemTool::finish(ErrorCode errorCode)
{
    emit finished(errorCode);
}

void QnConnectToCurrentSystemTool::mergeServer()
{
    NX_ASSERT(!m_mergeTool);

    m_mergeError = utils::MergeSystemsStatus::ok;
    m_mergeErrorMessage.clear();

    const auto server = resourcePool()->getIncompatibleResourceById(m_targetId, true);
    if (!server)
    {
        const auto compatibleServer =
            resourcePool()->getResourceById<QnMediaServerResource>(m_originalTargetId);
        if (compatibleServer && compatibleServer->getStatus() == Qn::Online)
        {
            finish(NoError);
            return;
        }

        m_mergeError = utils::MergeSystemsStatus::unknownError;
        finish(MergeFailed);
        return;
    }

    const auto ecServer = commonModule()->currentServer();
    if (!ecServer)
        return;

    QAuthenticator auth;
    auth.setUser(helpers::kFactorySystemUser);
    auth.setPassword(m_adminPassword);

    emit progressChanged(0);
    emit stateChanged(tr("Configuring Server"));

    m_mergeTool = new QnMergeSystemsTool(this);

    connect(m_mergeTool, &QnMergeSystemsTool::mergeFinished, this,
        [this](
            utils::MergeSystemsStatus::Value mergeStatus,
            const QnModuleInformation& moduleInformation)
        {
            if (mergeStatus != utils::MergeSystemsStatus::ok)
            {
                delete m_mergeTool;
                m_mergeError = mergeStatus;
                m_mergeErrorMessage =
                    utils::MergeSystemsStatus::getErrorMessage(mergeStatus, moduleInformation);
                finish(MergeFailed);
                return;
            }

            waitServer();
        });

    m_mergeTool->configureIncompatibleServer(ecServer, server->getApiUrl(), auth);
}

void QnConnectToCurrentSystemTool::waitServer()
{
    NX_ASSERT(m_mergeTool);

    auto finishMerge = [this](bool success)
        {
            resourcePool()->disconnect(this);
            delete m_mergeTool;
            finish(success ? NoError : MergeFailed);
        };

    if (resourcePool()->getIncompatibleResourceById(m_targetId, true))
    {
        finishMerge(true);
        return;
    }

    auto handleResourceChanged = [this, finishMerge](const QnResourcePtr& resource)
        {
            if (resource->getId() == m_originalTargetId && resource->getStatus() != Qn::Offline)
                finishMerge(true);
        };

    // Receiver object is m_mergeTool.
    // This helps us to break the connection when m_mergeTool is deleted in the cancel() method.
    connect(resourcePool(), &QnResourcePool::resourceAdded, m_mergeTool, handleResourceChanged);
    connect(resourcePool(), &QnResourcePool::statusChanged, m_mergeTool, handleResourceChanged);

    executeDelayedParented(
        [this, finishMerge]()
        {
            finishMerge(false);
        }, kWaitTimeout.count(), m_mergeTool);

    emit progressChanged(kUpdateProgress);
}

void QnConnectToCurrentSystemTool::updateServer()
{
    NX_ASSERT(!m_updateTool);

    const auto server = resourcePool()->getIncompatibleResourceById(m_targetId, true);

    if (server->getModuleInformation().protoVersion == QnAppInfo::ec2ProtoVersion())
    {
        emit progressChanged(kUpdateProgress);
        mergeServer();
        return;
    }

    emit stateChanged(tr("Updating Server"));
    emit progressChanged(kEmptyProgress);

    m_updateTool = new QnMediaServerUpdateTool(this);

    connect(m_updateTool, &QnMediaServerUpdateTool::updateFinished, this,
        [this](const QnUpdateResult& result)
        {
            m_updateTool->deleteLater();
            m_updateTool.clear();

            m_updateResult = result;

            if (result.result != QnUpdateResult::Successful)
            {
                finish(UpdateFailed);
                return;
            }

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

    auto targetVersion = qnStaticCommon->engineVersion();
    if (const auto ecServer = commonModule()->currentServer())
        targetVersion = ecServer->getVersion();

    m_updateTool->setTargets({m_targetId});
    m_updateTool->startUpdate(targetVersion);
}
