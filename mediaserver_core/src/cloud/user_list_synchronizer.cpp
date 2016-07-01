/**********************************************************
* Jun 30, 2016
* akolesnikov
***********************************************************/

#include "user_list_synchronizer.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>

#include "cloud_connection_manager.h"


using namespace nx::cdb;

CloudUserListSynchonizer::CloudUserListSynchonizer(
    CloudConnectionManager* const cloudConnectionManager)
:
    m_cloudConnectionManager(cloudConnectionManager),
    m_retryTimer(
        nx::network::RetryPolicy(
            nx::network::RetryPolicy::kInfiniteRetries,
            nx::network::RetryPolicy::kDefaultInitialDelay,
            nx::network::RetryPolicy::kDefaultDelayMultiplier,
            nx::network::RetryPolicy::kDefaultMaxDelay))
{
    m_retryTimer.bindToAioThread(getAioThread());

    Qn::directConnect(
        m_cloudConnectionManager, &CloudConnectionManager::connectedToCloud,
        this, &CloudUserListSynchonizer::onConnectedToCloud);
    Qn::directConnect(
        m_cloudConnectionManager, &CloudConnectionManager::disconnectedFromCloud,
        this, &CloudUserListSynchonizer::onDisconnectedFromCloud);

    QObject::connect(
        qnResPool, &QnResourcePool::resourceAdded,
        this, &CloudUserListSynchonizer::onResourceAdded,
        Qt::DirectConnection);
    QObject::connect(
        qnResPool, &QnResourcePool::resourceRemoved,
        this, &CloudUserListSynchonizer::onResourceRemoved,
        Qt::DirectConnection);
    QObject::connect(
        qnResPool, &QnResourcePool::resourceChanged,
        this, &CloudUserListSynchonizer::onResourceChanged,
        Qt::DirectConnection);
}

CloudUserListSynchonizer::~CloudUserListSynchonizer()
{
    directDisconnectAll();
    m_connection.reset();
    pleaseStopSync();
}

void CloudUserListSynchonizer::stopWhileInAioThread()
{
    m_retryTimer.pleaseStopSync();
}

void CloudUserListSynchonizer::executeNextTaskInLine()
{
    if (m_taskQueue.empty() || !m_connection)
        return;
    syncAllCloudUsers();
}

void CloudUserListSynchonizer::syncAllCloudUsers()
{
    const auto userResourceList = qnResPool->getResources<QnUserResource>();
    api::SystemSharingList cloudUserList;
    for (const auto& user: userResourceList)
    {
        if (!user->isCloud())
            continue;

        api::SystemSharing systemSharing;
        systemSharing.accountEmail = user->getName().toStdString();
        systemSharing.systemID = qnGlobalSettings->cloudSystemID().toStdString();
        systemSharing.accessRole = permissionsToCloudAccessRole(user);
        cloudUserList.sharing.push_back(std::move(systemSharing));
    }

    NX_LOGX(lit("Syncing all cloud users to the cloud"), cl_logDEBUG2);
    using namespace std::placeholders;
    //uploading full user list
    m_connection->systemManager()->setSystemUserList(
        std::move(cloudUserList),
        std::bind(&CloudUserListSynchonizer::onUserModificationsSaved, this, _1));
}

void CloudUserListSynchonizer::onConnectedToCloud()
{
    dispatch(
        [this]
        {
            NX_ASSERT(m_connection);
            m_connection = m_cloudConnectionManager->getCloudConnection();
            m_taskQueue.clear();
            m_taskQueue.push_back(TaskType::resyncAllUsers);
            executeNextTaskInLine();
        });
}

void CloudUserListSynchonizer::onDisconnectedFromCloud()
{
    std::unique_ptr<nx::cdb::api::Connection> connection;
    nx::utils::promise<void> connectionResetPromise;
    dispatch(
        [this, &connection, &connectionResetPromise]
        {
            m_retryTimer.pleaseStopSync();
            connection = std::move(m_connection);
            m_connection = nullptr;
            connectionResetPromise.set_value();
        });
    connectionResetPromise.get_future().wait();
    connection.reset();
}

void CloudUserListSynchonizer::onResourceAdded(const QnResourcePtr& resource)
{
    onResourceChanged(resource);
}

void CloudUserListSynchonizer::onResourceRemoved(const QnResourcePtr& resource)
{
    onResourceChanged(resource);
}

void CloudUserListSynchonizer::onResourceChanged(const QnResourcePtr& resource)
{
    if (!resource.dynamicCast<QnUserResource>())
        return;

    dispatch(
        [this]
        {
            m_taskQueue.push_back(TaskType::resyncAllUsers);
            if (m_taskQueue.size() == 1)
                executeNextTaskInLine();
        });
}

void CloudUserListSynchonizer::onUserModificationsSaved(
    api::ResultCode resultCode)
{
    dispatch(
        [this, resultCode]
        {
            m_taskQueue.pop_front();
            if (resultCode != api::ResultCode::ok)
            {
                NX_LOGX(lm("Failed to sync users to the cloud. %1")
                    .arg(api::toString(resultCode)), cl_logDEBUG1);
                //retrying
                m_retryTimer.scheduleNextTry(
                    [this]()
                    {
                        m_taskQueue.push_back(TaskType::resyncAllUsers);
                        if (m_taskQueue.size() == 1)
                            executeNextTaskInLine();
                    });
                return;
            }

            executeNextTaskInLine();
        });
}

nx::cdb::api::SystemAccessRole CloudUserListSynchonizer::permissionsToCloudAccessRole(
    const QnUserResourcePtr& user)
{
    if (!user->isEnabled())
        return nx::cdb::api::SystemAccessRole::disabled;

    const auto permissions = qnResourceAccessManager->globalPermissions(user);
    switch (permissions)
    {
        case Qn::GlobalAdminPermissionsSet:
            return nx::cdb::api::SystemAccessRole::localAdmin;
        case Qn::GlobalAdvancedViewerPermissionSet:
            return nx::cdb::api::SystemAccessRole::advancedViewer;
        case Qn::GlobalViewerPermissionSet:
            return nx::cdb::api::SystemAccessRole::viewer;
        case Qn::GlobalLiveViewerPermissionSet:
            return nx::cdb::api::SystemAccessRole::liveViewer;
        default:
            return nx::cdb::api::SystemAccessRole::custom;
    }
}
