#pragma once

#include <transaction/transaction.h>

#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/managers/abstract_videowall_manager.h>

namespace ec2
{
    class QnVideowallNotificationManager: public AbstractVideowallManager
    {
    public:
        QnVideowallNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiVideowallData>& tran )
        {
            assert(tran.command == ApiCommand::saveVideowall);
            emit addedOrUpdated(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert(tran.command == ApiCommand::removeVideowall);
            emit removed(tran.params.id);
        }

        void triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran)
        {
            assert(tran.command == ApiCommand::videowallControl);
            emit controlMessage(tran.params);
        }

    };

    template<class QueryProcessorType>
    class QnVideowallManager: public QnVideowallNotificationManager
    {
    public:
        QnVideowallManager( QueryProcessorType* const queryProcessor );

    protected:
        virtual int getVideowalls( impl::GetVideowallsHandlerPtr handler ) override;
        virtual int save(const ec2::ApiVideowallData& resource, impl::SimpleHandlerPtr handler) override;
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

        virtual int sendControlMessage(const ec2::ApiVideowallControlMessageData& message, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiVideowallData> prepareTransaction(ApiCommand::Value command, const ec2::ApiVideowallData& videowall);
        QnTransaction<ApiIdData> prepareTransaction(ApiCommand::Value command, const QnUuid& id);
        QnTransaction<ApiVideowallControlMessageData> prepareTransaction(ApiCommand::Value command, const ec2::ApiVideowallControlMessageData& message);
    };
}
