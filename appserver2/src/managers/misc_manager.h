#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_system_name_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnMiscNotificationManager : public AbstractMiscNotificationManager
    {
    public:
        void triggerNotification(const QnTransaction<ApiSystemIdData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiMiscData> &transaction);
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

        virtual int saveMiscParam(const ec2::ApiMiscData& params, impl::SimpleHandlerPtr handler) override;
        virtual int getMiscParam(const QByteArray& paramName, impl::GetMiscParamHandlerPtr handler) override;

        virtual int saveSystemMergeHistoryRecord(const ApiSystemMergeHistoryRecord& param, impl::SimpleHandlerPtr handler) override;
        virtual int getSystemMergeHistory(impl::GetSystemMergeHistoryHandlerPtr handler) override;

        virtual int saveRuntimeInfo(const ec2::ApiRuntimeData& data, impl::SimpleHandlerPtr handler) override;
    protected:
        virtual int changeSystemId(const QnUuid& systemId, qint64 sysIdTime, Timestamp tranLogTime, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };

} // namespace ec2
