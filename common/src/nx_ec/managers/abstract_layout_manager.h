#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2
{
    /*!
    \note All methods are asynchronous if other not specified
    */
    class AbstractLayoutManager: public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractLayoutManager() {}

        /*!
        \param handler Functor with params: (ErrorCode, const ApiLayoutDataList&)
        */
        template<class TargetType, class HandlerType>
        int getLayouts(TargetType* target, HandlerType handler)
        {
            return getLayouts(std::static_pointer_cast<impl::GetLayoutsHandler>(
                std::make_shared<impl::CustomGetLayoutsHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getLayoutsSync(ec2::ApiLayoutDataList* const layoutsList)
        {
            return impl::doSyncCall<impl::GetLayoutsHandler>([this](impl::GetLayoutsHandlerPtr handler)
            {
                this->getLayouts(handler);
            }, layoutsList);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int save(const ec2::ApiLayoutData& layout, TargetType* target, HandlerType handler)
        {
            return save(layout, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int remove(const QnUuid& layoutId, TargetType* target, HandlerType handler)
        {
            return remove(layoutId, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

    signals:
        void addedOrUpdated(const ec2::ApiLayoutData& layout);
        void removed(const QnUuid& id);

    protected:
        virtual int getLayouts(impl::GetLayoutsHandlerPtr handler) = 0;
        virtual int save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler) = 0;
        virtual int remove(const QnUuid& layoutId, impl::SimpleHandlerPtr handler) = 0;
    };
}