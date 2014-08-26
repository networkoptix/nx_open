#ifndef EC2_UPDATES_MANAGER_H
#define EC2_UPDATES_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_update_data.h"
#include "transaction/transaction.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"

namespace ec2 {

    class QnUpdatesNotificationManager
    :
        public AbstractUpdatesManager
    {
    public:
        QnUpdatesNotificationManager();

        void triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction);
        void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction);
        void triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction);

    private:
        QHash<QnAbstractTransaction::PersistentInfo, QString> m_requestedUpdateIds;

        void at_transactionProcessed(const QnAbstractTransaction &transaction);
    };


    template<class QueryProcessorType>
    class QnUpdatesManager
    :
        public QnUpdatesNotificationManager
    {
    public:
        QnUpdatesManager(QueryProcessorType * const queryProcessor);
        virtual ~QnUpdatesManager();

    protected:
        virtual int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) override;
        virtual int sendUpdateUploadResponce(const QString &updateId, const QUuid &peerId, int chunks, impl::SimpleHandlerPtr handler) override;
        virtual int installUpdate(const QString &updateId, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiUpdateUploadData> prepareTransaction(const QString &updateId, const QByteArray &data, qint64 offset) const;
        QnTransaction<ApiUpdateUploadResponceData> prepareTransaction(const QString &updateId, const QUuid &peerId, int chunks) const;
        QnTransaction<ApiUpdateInstallData> prepareTransaction(const QString &updateId) const;
    };

} // namespace ec2

#endif // EC2_UPDATES_MANAGER_H
