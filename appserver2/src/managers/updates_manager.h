#ifndef EC2_UPDATES_MANAGER_H
#define EC2_UPDATES_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_update_data.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction.h"

namespace ec2 {

    class QnUpdatesNotificationManager : public AbstractUpdatesNotificationManager
    {
    public:
        QnUpdatesNotificationManager();

        void triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction, NotificationSource source);

    private:
        void at_transactionProcessed(const QnAbstractTransaction &transaction);
    };

    typedef std::shared_ptr<QnUpdatesNotificationManager> QnUpdatesNotificationManagerPtr;
    typedef QnUpdatesNotificationManager *QnUpdatesNotificationManagerRawPtr;


    template<class QueryProcessorType>
    class QnUpdatesManager
    :
        public AbstractUpdatesManager
    {
    public:
        QnUpdatesManager(
            QueryProcessorType * const queryProcessor,
            const Qn::UserAccessData &userAccessData,
            TransactionMessageBusAdapter* messageBus);
        virtual ~QnUpdatesManager();

    protected:
        virtual int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) override;
        virtual int sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId, int chunks, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
        TransactionMessageBusAdapter* m_messageBus;
    };

} // namespace ec2

#endif // EC2_UPDATES_MANAGER_H
