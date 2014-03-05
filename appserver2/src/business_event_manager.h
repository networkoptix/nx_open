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
        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) override;
        virtual int deleteRule( QnId ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

        template<class T> void triggerNotification( const QnTransaction<T>& tran ) {
            static_assert( false, "Specify QnBusinessEventManager::triggerNotification<>, please" );
        }

        template<> void triggerNotification<ApiBusinessActionData>( const QnTransaction<ApiBusinessActionData>& tran )
        {
            assert( tran.command == ApiCommand::broadcastBusinessAction );
            QnAbstractBusinessActionPtr businessAction = tran.params.toResource( m_resCtx.pool );
            emit gotBroadcastAction( businessAction );
        }

        template<> void triggerNotification<ApiIdData>( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeBusinessRule );
            emit removed( QnId(tran.params.id) );
        }

        template<> void triggerNotification<ApiBusinessRuleData>( const QnTransaction<ApiBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::saveBusinessRule);
            QnBusinessEventRulePtr businessRule( new QnBusinessEventRule() );
            tran.params.toResource( businessRule, m_resCtx.pool );
            emit addedOrUpdated( businessRule );
        }

        template<> void triggerNotification<ApiResetBusinessRuleData>( const QnTransaction<ApiResetBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::resetBusinessRules);
            emit businessRuleReset( tran.params.defaultRules.toResourceList(m_resCtx.pool) );
        }
        

    private:
        QueryProcessorType* const m_queryProcessor;
        ResourceContext m_resCtx;

        QnTransaction<ApiBusinessRuleData> prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnId& id );
        QnTransaction<ApiBusinessActionData> prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
