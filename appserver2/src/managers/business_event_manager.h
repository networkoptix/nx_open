#ifndef __BUSINESS_RULE_MANAGER_H_
#define __BUSINESS_RULE_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "business/business_event_rule.h"

namespace ec2
{
    class QnBusinessEventNotificationManager : public AbstractBusinessEventNotificationManager
    {
    public:
        QnBusinessEventNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran, NotificationSource /*source*/)
        {
            NX_ASSERT( tran.command == ApiCommand::broadcastAction || tran.command == ApiCommand::execAction);
            QnAbstractBusinessActionPtr businessAction;
            fromApiToResource(tran.params, businessAction);
            businessAction->setReceivedFromRemoteHost(true);
            if (tran.command == ApiCommand::broadcastAction)
                emit gotBroadcastAction( businessAction );
            else
                emit execBusinessAction( businessAction );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
        {
            NX_ASSERT( tran.command == ApiCommand::removeEventRule );
            emit removed( QnUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran, NotificationSource source)
        {
            NX_ASSERT( tran.command == ApiCommand::saveEventRule);
            QnBusinessEventRulePtr businessRule( new QnBusinessEventRule() );
            fromApiToResource(tran.params, businessRule);
            emit addedOrUpdated( businessRule, source);
        }

        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran, NotificationSource /*source*/)
        {
            NX_ASSERT( tran.command == ApiCommand::resetEventRules);
            emit businessRuleReset(tran.params.defaultRules);
        }
    };

    typedef std::shared_ptr<QnBusinessEventNotificationManager> QnBusinessEventNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnBusinessEventManager : public AbstractBusinessEventManager
    {
    public:
        QnBusinessEventManager(
            QnTransactionMessageBusBase* messageBus,
            QueryProcessorType* const queryProcessor,
            const Qn::UserAccessData &userAccessData);

        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) override;


        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) override;
        virtual int deleteRule( QnUuid ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual int sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QnUuid& dstPeer, impl::SimpleHandlerPtr handler ) override;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

    private:
        QnTransactionMessageBusBase* m_messageBus;
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiBusinessActionData> prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
