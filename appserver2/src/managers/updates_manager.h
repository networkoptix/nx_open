#ifndef EC2_UPDATES_MANAGER_H
#define EC2_UPDATES_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_update_data.h"
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

protected:
    virtual int sendUpdatePackage(const QString &updateId, const QByteArray &data, const PeerList &peers, impl::SimpleHandlerPtr handler) override;
    virtual int sendUpdateUploadedResponce(const QString &updateId, const QnId &peerId, impl::SimpleHandlerPtr handler) override;
    virtual int installUpdate(const QString &updateId, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;

    QnTransaction<ApiUpdateUploadData> prepareTransaction(const QString &updateId, const QByteArray &data) const;
    QnTransaction<ApiUpdateUploadResponceData> prepareTransaction(const QString &updateId, const QnId &peerId) const;
    QnTransaction<ApiUpdateInstallData> prepareTransaction(const QString &updateId) const;
};

} // namespace ec2

#endif // EC2_UPDATES_MANAGER_H
