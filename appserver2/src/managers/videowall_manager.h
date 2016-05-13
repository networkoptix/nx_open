#pragma once

#include <memory>
#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <core/resource_management/user_access_data.h>

namespace ec2
{
    class QnVideowallNotificationManager: public AbstractVideowallManagerBase
    {
    public:
        QnVideowallNotificationManager();
        void triggerNotification( const QnTransaction<ApiVideowallData>& tran );
        void triggerNotification( const QnTransaction<ApiIdData>& tran );
        void triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran);
    };

    typedef std::shared_ptr<QnVideowallNotificationManager> QnVideowallNotificationManagerPtr;
    typedef QnVideowallNotificationManager *QnVideowallNotificationManagerRawPtr;

    template<class QueryProcessorType>
    class QnVideowallManager: public AbstractVideowallManager
    {
    public:
        QnVideowallManager(QnVideowallNotificationManagerRawPtr &base,
                           QueryProcessorType* const queryProcessor,
                           const Qn::UserAccessData &userAccessData = Qn::kSuperUserAccess);

        QnVideowallNotificationManagerRawPtr getBase() const override { return m_base; }

    protected:
        virtual int getVideowalls( impl::GetVideowallsHandlerPtr handler ) override;
        virtual int save(const ec2::ApiVideowallData& resource, impl::SimpleHandlerPtr handler) override;
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

        virtual int sendControlMessage(const ec2::ApiVideowallControlMessageData& message, impl::SimpleHandlerPtr handler) override;

    private:
        QnVideowallNotificationManagerRawPtr m_base;
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}
