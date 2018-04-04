#include "updates_manager.h"

#include <transaction/transaction_message_bus.h>
#include <utils/common/scoped_thread_rollback.h>
#include <nx/utils/concurrent.h>

#include "ec2_thread_pool.h"

namespace ec2 {


    ////////////////////////////////////////////////////////////
    //// class QnUpdatesNotificationManager
    ////////////////////////////////////////////////////////////
    QnUpdatesNotificationManager::QnUpdatesNotificationManager()
    {
    }

    void QnUpdatesNotificationManager::triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction, NotificationSource /*source*/)
    {
        NX_ASSERT(transaction.command == ApiCommand::uploadUpdate);
        emit updateChunkReceived(transaction.params.updateId, transaction.params.data, transaction.params.offset);
    }

    void QnUpdatesNotificationManager::triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction, NotificationSource /*source*/)
    {
        NX_ASSERT(transaction.command == ApiCommand::uploadUpdateResponce);
        emit updateUploadProgress(transaction.params.updateId, transaction.params.id, transaction.params.chunks);
    }

    void QnUpdatesNotificationManager::triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction, NotificationSource /*source*/)
    {
        NX_ASSERT(transaction.command == ApiCommand::installUpdate);
        // TODO: #2.4 #rvasilenko #dklychkov Implement a mechanism to determine the transaction was successfully sent to the other peers, then emit this signal.
        emit updateInstallationRequested(transaction.params.updateId);
    }


} // namespace ec2
