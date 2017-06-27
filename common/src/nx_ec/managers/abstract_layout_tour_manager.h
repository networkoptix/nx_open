#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_layout_tour_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2 {

class AbstractLayoutTourNotificationManager: public QObject
{
    Q_OBJECT
signals:
    void addedOrUpdated(const ec2::ApiLayoutTourData& layout, NotificationSource source);
    void removed(const QnUuid& id);
};

typedef std::shared_ptr<AbstractLayoutTourNotificationManager> AbstractLayoutTourNotificationManagerPtr;

/*!
\note All methods are asynchronous if other not specified
*/
class AbstractLayoutTourManager
{
public:
    virtual ~AbstractLayoutTourManager() {}

    /*!
    \param handler Functor with params: (ErrorCode, const ApiLayoutTourDataList&)
    */
    template<class TargetType, class HandlerType>
    int getLayoutTours(TargetType* target, HandlerType handler)
    {
        return getLayoutTours(std::static_pointer_cast<impl::GetLayoutToursHandler>(
            std::make_shared<impl::CustomGetLayoutToursHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode getLayoutToursSync(ec2::ApiLayoutTourDataList* const layoutsList)
    {
        return impl::doSyncCall<impl::GetLayoutToursHandler>(
            [this](impl::GetLayoutToursHandlerPtr handler)
            {
                this->getLayoutTours(handler);
            }, layoutsList);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const ec2::ApiLayoutTourData& layout, TargetType* target, HandlerType handler)
    {
        return save(layout, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int remove(const QnUuid& tourId, TargetType* target, HandlerType handler)
    {
        return remove(tourId, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

protected:
    virtual int getLayoutTours(impl::GetLayoutToursHandlerPtr handler) = 0;
    virtual int save(const ec2::ApiLayoutTourData& tour, impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& tourId, impl::SimpleHandlerPtr handler) = 0;
};

} // namespace ec2
