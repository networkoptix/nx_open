/**********************************************************
* 27 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef TIME_MANAGER_API_H
#define TIME_MANAGER_API_H

#include <nx_ec/ec_api.h>


namespace ec2
{
    template<typename QueryProcessorType>
    class QnTimeNotificationManager : public AbstractTimeNotificationManager
    {
    public:
        QnTimeNotificationManager();
        ~QnTimeNotificationManager();
    };

    template<class QueryProcessorType>
    class QnTimeManager
    :
        public AbstractTimeManager
    {
    public:
        QnTimeManager(QueryProcessorType* queryProcessor, const Qn::UserAccessData &userAccessData );
        virtual ~QnTimeManager();

        //!Implementation of AbstractTimeManager::getPeerTimeInfoList
        virtual QnPeerTimeInfoList getPeerTimeInfoList() const override;
        //!Implementation of AbstractTimeManager::getCurrentTimeImpl
        virtual int getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler ) override;
        //!Implementation of AbstractTimeManager::forcePrimaryTimeServerImpl
        virtual int forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractTimeManager::forceTimeResync
        virtual void forceTimeResync() override;

    private:
        QueryProcessorType* m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}

#endif  //TIME_MANAGER_API_H
