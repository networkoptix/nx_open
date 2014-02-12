#ifndef __BUSINESS_RULE_MANAGER_H_
#define __BUSINESS_RULE_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/ec2_business_rule_data.h"

namespace ec2
{
    template<class QueryProcessorType>
    class QnBusinessEventManager
        :
        public AbstractBusinessEventManager
    {
    public:
        QnBusinessEventManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) override;

        virtual int testEmailSettings( const QnKvPairList& settings, impl::SimpleHandlerPtr handler ) override;
        virtual int sendEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, const QnEmailAttachmentList& attachments, impl::SimpleHandlerPtr handler ) override;
        virtual int save( const QnBusinessEventRulePtr& rule, impl::SimpleHandlerPtr handler ) override;
        virtual int deleteRule( int ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

        template<class T> void triggerNotification( const QnTransaction<T>& tran ) {
            static_assert( false, "Specify QnBusinessEventManager::triggerNotification<>, please" );
        }

        template<> void triggerNotification<ApiIdData>( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeBusinessRule );
            emit removed( QnId(tran.params.id) );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        ResourceContext m_resCtx;

        QnTransaction<ApiBusinessRuleData> prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
