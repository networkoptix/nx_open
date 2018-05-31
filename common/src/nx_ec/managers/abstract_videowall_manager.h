#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2 {

class AbstractVideowallNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(
        const nx::vms::api::VideowallData& videowall,
        ec2::NotificationSource source);
    void removed(const QnUuid& id);
    void controlMessage(const nx::vms::api::VideowallControlMessageData& message);
};

typedef std::shared_ptr<AbstractVideowallNotificationManager>
AbstractVideowallNotificationManagerPtr;

/*!
\note All methods are asynchronous if other not specified
*/
class AbstractVideowallManager
{
public:
    virtual ~AbstractVideowallManager()
    {
    }

    /*!
    \param handler Functor with params: (ErrorCode, const nx::vms::api::VideowallDataList&)
    */
    template<class TargetType, class HandlerType>
    int getVideowalls(TargetType* target, HandlerType handler)
    {
        return getVideowalls(
            std::static_pointer_cast<impl::GetVideowallsHandler>(
                std::make_shared<impl::CustomGetVideowallsHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getVideowallsSync(nx::vms::api::VideowallDataList* const videowallList)
    {
        return impl::doSyncCall<impl::GetVideowallsHandler>(
            [this](impl::GetVideowallsHandlerPtr handler)
            {
                this->getVideowalls(handler);
            },
            videowallList);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const nx::vms::api::VideowallData& videowall, TargetType* target, HandlerType handler)
    {
        return save(
            videowall,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int remove(const QnUuid& id, TargetType* target, HandlerType handler)
    {
        return remove(
            id,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int sendControlMessage(
        const nx::vms::api::VideowallControlMessageData& message,
        TargetType* target,
        HandlerType handler)
    {
        return sendControlMessage(
            message,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

protected:
    virtual int getVideowalls(impl::GetVideowallsHandlerPtr handler) = 0;
    virtual int save(
        const nx::vms::api::VideowallData& videowall,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

    virtual int sendControlMessage(
        const nx::vms::api::VideowallControlMessageData& message,
        impl::SimpleHandlerPtr handler) = 0;
};

} // namespace ec2
