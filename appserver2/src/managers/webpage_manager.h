#pragma once

#include <core/resource/resource_fwd.h>

#include <nx_ec/ec_api.h>

#include <transaction/transaction.h>

namespace ec2
{
    class QnWebPageNotificationManager: public AbstractWebPageManager
    {
    public:
        QnWebPageNotificationManager();

        void triggerNotification( const QnTransaction<ApiWebPageData>& tran );
        void triggerNotification( const QnTransaction<ApiIdData>& tran );
    };

    template<class QueryProcessorType>
    class QnWebPageManager: public QnWebPageNotificationManager
    {
    public:
        QnWebPageManager( QueryProcessorType* const queryProcessor);

    protected:
        virtual int getWebPages( impl::GetWebPagesHandlerPtr handler ) override;
        virtual int save( const QnWebPageResourcePtr& resource, impl::AddWebPageHandlerPtr handler ) override;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiWebPageData> prepareTransaction(ApiCommand::Value command, const QnWebPageResourcePtr &resource);
        QnTransaction<ApiIdData> prepareTransaction(ApiCommand::Value command, const QnUuid& id);
    };
}
