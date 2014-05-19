#ifndef EC2_UPDATES_MANAGER_H
#define EC2_UPDATES_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_update_data.h"
#include "transaction/transaction.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"

namespace ec2 {

template<class QueryProcessorType>
class QnUpdatesManager : public AbstractUpdatesManager {
public:
    QnUpdatesManager(QueryProcessorType * const queryProcessor);
    virtual ~QnUpdatesManager();

    void triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction);
    void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction);
    void triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction);
    void triggerNotification(const QnTransaction<ApiUpdateUploadAndInstallData> &transaction);

protected:
    virtual int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const PeerList &peers, impl::SimpleHandlerPtr handler) override;
    virtual int sendUpdateUploadResponce(const QString &updateId, const QnId &peerId, int chunks, impl::SimpleHandlerPtr handler) override;
    virtual int installUpdate(const QString &updateId, const PeerList &peers, impl::SimpleHandlerPtr handler) override;
    virtual int sendAndInstallUpdate(const QString &updateId, const QByteArray &data, const QString &targetId, const QnId &peerId, impl::SimpleHandlerPtr handler);

private:
    QueryProcessorType* const m_queryProcessor;

    QHash<QnAbstractTransaction::ID, QString> m_requestedUpdateIds;

    QnTransaction<ApiUpdateUploadData> prepareTransaction(const QString &updateId, const QByteArray &data, qint64 offset) const;
    QnTransaction<ApiUpdateUploadResponceData> prepareTransaction(const QString &updateId, const QnId &peerId, int chunks) const;
    QnTransaction<ApiUpdateInstallData> prepareTransaction(const QString &updateId) const;
    QnTransaction<ApiUpdateUploadAndInstallData> prepareTransaction(const QString &updateId, const QByteArray &data, const QString &targetId) const;

    void at_transactionProcessed(const QnAbstractTransaction &transaction);
};

} // namespace ec2

#endif // EC2_UPDATES_MANAGER_H
