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
        QnBusinessEventNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran )
        {
            assert( tran.command == ApiCommand::broadcastBusinessAction || tran.command == ApiCommand::execBusinessAction);
            QnAbstractBusinessActionPtr businessAction;
            fromApiToResource(tran.params, businessAction);
            businessAction->setReceivedFromRemoteHost(true);
            if (tran.command == ApiCommand::broadcastBusinessAction)
                emit gotBroadcastAction( businessAction );
            else
                emit execBusinessAction( businessAction );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeBusinessRule );
            emit removed( QnUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::saveBusinessRule);
            QnBusinessEventRulePtr businessRule( new QnBusinessEventRule() );
            fromApiToResource(tran.params, businessRule);
            emit addedOrUpdated( businessRule );
        }

        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran )
        {
            assert( tran.command == ApiCommand::resetBusinessRules);
            emit businessRuleReset(tran.params.defaultRules);
        }
    };




    template<class QueryProcessorType>
    class QnBusinessEventManager
    :
        public QnBusinessEventNotificationManager
    {
    public:
        QnBusinessEventManager( QueryProcessorType* const queryProcessor );

        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) override;


        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) override;
        virtual int deleteRule( QnUuid ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual int sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QnUuid& dstPeer, impl::SimpleHandlerPtr handler ) override;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiBusinessRuleData> prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& id );
        QnTransaction<ApiBusinessActionData> prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
