/**********************************************************
* Jun 30, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <deque>

#include <cdb/connection.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/retry_timer.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/safe_direct_connection.h>


class CloudConnectionManager;

/** Responsible for keeping user list in cloud in sync with the system */
class CloudUserListSynchonizer
:
    public QObject,
    public nx::network::aio::BasicPollable,
    public Qn::EnableSafeDirectConnection
{
public:
    CloudUserListSynchonizer(CloudConnectionManager* const cloudConnectionManager);
    ~CloudUserListSynchonizer();

    virtual void stopWhileInAioThread() override;

private:
    enum class TaskType
    {
        resyncAllUsers
    };

    CloudConnectionManager* const m_cloudConnectionManager;
    std::unique_ptr<nx::cdb::api::Connection> m_connection;
    std::deque<TaskType> m_taskQueue;
    nx::network::RetryTimer m_retryTimer;

    void executeNextTaskInLine();
    void syncAllCloudUsers();
    void onConnectedToCloud();
    void onDisconnectedFromCloud();
    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onResourceChanged(const QnResourcePtr& resource);
    void onUserModificationsSaved(nx::cdb::api::ResultCode resultCode);

    static nx::cdb::api::SystemAccessRole permissionsToCloudAccessRole(
        const QnUserResourcePtr& user);
};
