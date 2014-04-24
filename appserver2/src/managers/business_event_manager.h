#ifndef __BUSINESS_RULE_MANAGER_H_
#define __BUSINESS_RULE_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_business_rule_data.h"

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

        virtual int testEmailSettings( const QnEmail::Settings& settings, impl::SimpleHandlerPtr handler ) override;
        virtual int sendEmail(const ApiEmailData& data, impl::SimpleHandlerPtr handler ) override;
        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) override;
        virtual int deleteRule( QnId ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran )
        {
            assert( tran.command == ApiCommand::broadcastBusinessAction );
            QnAbstractBusinessActionPtr businessAction = tran.params.toResource( m_resCtx.pool );
            emit gotBroadcastAction( businessAction );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeBusinessRule );
            emit removed( QnId(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::saveBusinessRule);
            QnBusinessEventRulePtr businessRule( new QnBusinessEventRule() );
            tran.params.toResource( businessRule, m_resCtx.pool );
            emit addedOrUpdated( businessRule );
        }

        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran )
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
        QnTransaction<ApiEmailSettingsData> prepareTransaction( ApiCommand::Value command, const QnEmail::Settings& resource );
        QnTransaction<ApiEmailData> prepareTransaction( ApiCommand::Value command, const ApiEmailData& data );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
