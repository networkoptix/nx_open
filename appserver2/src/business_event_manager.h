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
        QnBusinessEventManager( QueryProcessorType* const queryProcessor, QSharedPointer<QnResourceFactory> factory, QnResourcePool* resourcePool );

        virtual ReqID getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) override;

        virtual ReqID addBusinessRule( const QnBusinessEventRulePtr& businessRule, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID testEmailSettings( const QnKvPairList& settings, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID sendEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, const QnEmailAttachmentList& attachments, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID save( const QnBusinessEventRulePtr& rule, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID deleteRule( int ruleId, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID resetBusinessRules( impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        QSharedPointer<QnResourceFactory> m_resourcefactory;
        QnResourcePool* m_resourcePool;

        QnTransaction<ApiBusinessRuleData> prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource );
    };
}

#endif  // __BUSINESS_RULE_MANAGER_H_
