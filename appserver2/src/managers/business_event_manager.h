#ifndef __BUSINESS_RULE_MANAGER_H_
#define __BUSINESS_RULE_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "business/business_event_rule.h"

namespace ec2
{
    class QnBusinessEventNotificationManager
    :
        public AbstractBusinessEventManager
    {
    public:
        QnBusinessEventNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran )
        {
            assert( tran.command == ApiCommand::broadcastBusinessAction || tran.command == ApiCommand::execBusinessAction);
            QnAbstractBusinessActionPtr businessAction;
            fromApiToResource(tran.params, businessAction, m_resCtx.pool);
            businessAction->setReceivedFromRemoteHost(true);
            if (tran.command == ApiCommand::broadcastBusinessAction)
                emit gotBroadcastAction( businessAction );
            else
                emit execBusinessAction( businessAction );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeBusinessRule );
            emit removed( QUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::saveBusinessRule);
            QnBusinessEventRulePtr businessRule( new QnBusinessEventRule() );
            fromApiToResource(tran.params, businessRule, m_resCtx.pool);
            emit addedOrUpdated( businessRule );
        }

        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::resetBusinessRules);
            QnBusinessEventRuleList rules;
            fromApiToResourceList(tran.params.defaultRules, rules, m_resCtx.pool);
            emit businessRuleReset( rules );
        }

    protected:
        ResourceContext m_resCtx;
    };




    template<class QueryProcessorType>
    class QnBusinessEventManager
    :
        public QnBusinessEventNotificationManager
    {
    public:
        QnBusinessEventManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) override;


        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) override;
        virtual int deleteRule( QUuid ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual int sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QUuid& dstPeer, impl::SimpleHandlerPtr handler ) override;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiBusinessRuleData> prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QUuid& id );
        QnTransaction<ApiBusinessActionData> prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource );
        QnTransaction<ApiEmailSettingsData> prepareTransaction( ApiCommand::Value command, const QnEmail::Settings& resource );
        QnTransaction<ApiEmailData> prepareTransaction( ApiCommand::Value command, const ApiEmailData& data );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
