
#ifndef __USER_MANAGER_H_
#define __USER_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/ec2_user_data.h"

namespace ec2
{
    template<class QueryProcessorType>
    class QnUserManager
        :
        public AbstractUserManager
    {
    public:
        QnUserManager( QueryProcessorType* const queryProcessor, QSharedPointer<QnResourceFactory> factory );

        virtual ReqID getUsers( impl::GetUsersHandlerPtr handler ) override;
        virtual ReqID save( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID remove( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;


    private:
        QueryProcessorType* const m_queryProcessor;
        QSharedPointer<QnResourceFactory> m_resourcefactory;

        QnTransaction<ApiUserData> prepareTransaction( ApiCommand::Value command, const QnUserResourcePtr& resource );
    };
}

#endif  // __USER_MANAGER_H_
