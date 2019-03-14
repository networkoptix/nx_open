#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2 {

class AbstractLayoutNotificationManager: public QObject
{
Q_OBJECT
public:

signals :
    void addedOrUpdated(const nx::vms::api::LayoutData& layout, ec2::NotificationSource source);
    void removed(const QnUuid& id);
};

typedef std::shared_ptr<AbstractLayoutNotificationManager> AbstractLayoutNotificationManagerPtr;

/*!
\note All methods are asynchronous if other not specified
*/
class AbstractLayoutManager
{
public:
    virtual ~AbstractLayoutManager()
    {
    }

    /*!
    \param handler Functor with params: (ErrorCode, const nx::vms::api::LayoutDataList&)
    */
    template<class TargetType, class HandlerType>
    int getLayouts(TargetType* target, HandlerType handler)
    {
        return getLayouts(
            std::static_pointer_cast<impl::GetLayoutsHandler>(
                std::make_shared<impl::CustomGetLayoutsHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getLayoutsSync(nx::vms::api::LayoutDataList* const layoutsList)
    {
        return impl::doSyncCall<impl::GetLayoutsHandler>(
            [this](impl::GetLayoutsHandlerPtr handler)
            {
                this->getLayouts(handler);
            },
            layoutsList);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const nx::vms::api::LayoutData& layout, TargetType* target, HandlerType handler)
    {
        return save(
            layout,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode saveSync(const nx::vms::api::LayoutData& layout)
    {
        int (AbstractLayoutManager::* fn)(
                const nx::vms::api::LayoutData&,
                impl::SimpleHandlerPtr) = &AbstractLayoutManager::save;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, layout, std::placeholders::_1));
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int remove(const QnUuid& layoutId, TargetType* target, HandlerType handler)
    {
        return remove(
            layoutId,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

protected:
    virtual int getLayouts(impl::GetLayoutsHandlerPtr handler) = 0;
    virtual int save(const nx::vms::api::LayoutData& layout, impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& layoutId, impl::SimpleHandlerPtr handler) = 0;
};
}
