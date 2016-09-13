#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_system_name_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnMiscNotificationManager : public AbstractMiscNotificationManager
    {
    public:
        void triggerNotification(const QnTransaction<ApiSystemNameData> &transaction);
    };

    typedef std::shared_ptr<QnMiscNotificationManager> QnMiscNotificationManagerPtr;
    typedef QnMiscNotificationManager *QnMiscNotificationManagerRawPtr;

    template<class QueryProcessorType>
    class QnMiscManager : public AbstractMiscManager
    {
    public:
        QnMiscManager(QueryProcessorType * const queryProcessor, const Qn::UserAccessData &userAccessData);

        virtual ~QnMiscManager();
        virtual int markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) override;
        virtual int cleanupDatabase(bool cleanupDbObjects, bool cleanupTransactionLog, impl::SimpleHandlerPtr handler) override;

    protected:
        virtual int changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiSystemNameData> prepareTransaction(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime) const;
    };

} // namespace ec2
