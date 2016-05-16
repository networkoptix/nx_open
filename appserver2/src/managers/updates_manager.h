#ifndef EC2_UPDATES_MANAGER_H
#define EC2_UPDATES_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_update_data.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction.h"

namespace ec2 {

    class QnUpdatesNotificationManager
    :
        public AbstractUpdatesManagerBase
    {
    public:
        QnUpdatesNotificationManager();

        void triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction);
        void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction);
        void triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction);

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
        QnUpdatesManager(QnUpdatesNotificationManagerRawPtr base, QueryProcessorType * const queryProcessor, const Qn::UserAccessData &userAccessData);
        virtual ~QnUpdatesManager();
        virtual QnUpdatesNotificationManagerRawPtr getBase() const override { return m_base; }

    protected:
        virtual int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) override;
        virtual int sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId, int chunks, impl::SimpleHandlerPtr handler) override;
        virtual int installUpdate(const QString &updateId, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) override;

    private:
        QnUpdatesNotificationManagerRawPtr m_base;
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiUpdateUploadData> prepareTransaction(const QString &updateId, const QByteArray &data, qint64 offset) const;
        QnTransaction<ApiUpdateUploadResponceData> prepareTransaction(const QString &updateId, const QnUuid &peerId, int chunks) const;
        QnTransaction<ApiUpdateInstallData> prepareTransaction(const QString &updateId) const;
    };

} // namespace ec2

#endif // EC2_UPDATES_MANAGER_H
