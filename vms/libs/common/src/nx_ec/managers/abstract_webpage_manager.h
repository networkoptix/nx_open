#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

#include <nx/vms/api/data/webpage_data.h>

namespace ec2 {

class AbstractWebPageNotificationManager: public QObject
{
Q_OBJECT
public:
signals :
    void addedOrUpdated(const nx::vms::api::WebPageData& webpage, ec2::NotificationSource source);
    void removed(const QnUuid& id);
};

typedef std::shared_ptr<AbstractWebPageNotificationManager> AbstractWebPageNotificationManagerPtr;

/*!
\note All methods are asynchronous if other not specified
*/
class AbstractWebPageManager
{
public:
    virtual ~AbstractWebPageManager()
    {
    }

    /*!
    \param handler Functor with params: (ErrorCode, const nx::vms::api::WebPageDataList&)
    */
    template<class TargetType, class HandlerType>
    int getWebPages(TargetType* target, HandlerType handler)
    {
        return getWebPages(
            std::static_pointer_cast<impl::GetWebPagesHandler>(
                std::make_shared<impl::CustomGetWebPagesHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getWebPagesSync(nx::vms::api::WebPageDataList* const webPageList)
    {
        return impl::doSyncCall<impl::GetWebPagesHandler>(
            [this](impl::GetWebPagesHandlerPtr handler)
            {
                this->getWebPages(handler);
            },
            webPageList);
    }

    /*!
    \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const nx::vms::api::WebPageData& resource, TargetType* target, HandlerType handler)
    {
        return save(
            resource,
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

protected:
    virtual int getWebPages(impl::GetWebPagesHandlerPtr handler) = 0;
    virtual int save(const nx::vms::api::WebPageData& webpage, impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;
};

} // namespace ec2
