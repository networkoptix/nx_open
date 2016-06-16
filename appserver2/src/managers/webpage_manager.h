#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <core/resource_management/user_access_data.h>

namespace ec2
{
    class QnWebPageNotificationManager: public AbstractWebPageNotificationManager
    {
    public:
        QnWebPageNotificationManager();

        void triggerNotification( const QnTransaction<ApiWebPageData>& tran );
        void triggerNotification( const QnTransaction<ApiIdData>& tran );
    };

    typedef std::shared_ptr<QnWebPageNotificationManager> QnWebPageNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnWebPageManager: public AbstractWebPageManager
    {
    public:
        QnWebPageManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

    protected:
        virtual int getWebPages( impl::GetWebPagesHandlerPtr handler ) override;
        virtual int save(const ec2::ApiWebPageData& webpage, impl::SimpleHandlerPtr handler) override;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}
