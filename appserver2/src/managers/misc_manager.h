#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_system_name_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnMiscNotificationManager : public AbstractMiscManagerBase
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
        QnMiscManager(QnMiscNotificationManagerRawPtr base,
                      QueryProcessorType * const queryProcessor,
                      const Qn::UserAccessData &userAccessData);

        virtual QnMiscNotificationManagerRawPtr getBase() const override { return m_base; }

        virtual ~QnMiscManager();
        virtual int markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) override;

    protected:
        virtual int changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime, impl::SimpleHandlerPtr handler) override;

    private:
        QnMiscNotificationManagerRawPtr m_base;
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiSystemNameData> prepareTransaction(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime) const;
    };

} // namespace ec2
