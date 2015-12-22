#pragma once

#include <core/resource/webpage_resource.h>

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_webpage_data.h"
#include "nx_ec/data/api_conversion_functions.h"

namespace ec2
{
    class QnWebPageNotificationManager
        :
        public AbstractWebPageManager
    {
    public:
        QnWebPageNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiWebPageData>& tran )
        {
            assert( tran.command == ApiCommand::saveWebPage);
            QnWebPageResourcePtr webPage(new QnWebPageResource());
            fromApiToResource(tran.params, webPage);
            emit addedOrUpdated( webPage );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeWebPage );
            emit removed( QnUuid(tran.params.id) );
        }

    protected:
        ResourceContext m_resCtx;
    };

    template<class QueryProcessorType>
    class QnWebPageManager
        :
        public QnWebPageNotificationManager
    {
    public:
        QnWebPageManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

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
