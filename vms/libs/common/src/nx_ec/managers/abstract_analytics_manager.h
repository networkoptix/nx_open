#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

#include <nx/vms/api/data/analytics_data.h>

namespace ec2 {

class AbstractAnalyticsNotificationManager: public QObject
{
    Q_OBJECT
public:
signals:
    void analyticsPluginAddedOrUpdated(
        const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
        ec2::NotificationSource source);

    void analyticsEngineAddedOrUpdated(
        const nx::vms::api::AnalyticsEngineData& analyticsEngine,
        ec2::NotificationSource source);

    void analyticsPluginRemoved(const QnUuid& id);

    void analyticsEngineRemoved(const QnUuid& id);
};

using AbstractAnalyticsNotificationManagerPtr =
    std::shared_ptr<AbstractAnalyticsNotificationManager>;

/**
 * NOTE: All methods are asynchronous, unless specified otherwise.
 */
class AbstractAnalyticsManager
{
public:
    virtual ~AbstractAnalyticsManager() = default;

    /**
     * @param handler Functor with params: (ErrorCode, const nx::vms::api::AnalyticsPluginDataList&)
     */
    template<class TargetType, class HandlerType>
    int getAnalyticsPlugins(TargetType* target, HandlerType handler)
    {
        return getAnalyticsPlugins(
            std::static_pointer_cast<impl::GetAnalyticsPluginsHandler>(
                std::make_shared<impl::CustomGetAnalyticsPluginsHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getAnalyticsPluginsSync(
        nx::vms::api::AnalyticsPluginDataList* const outAnalyticsPluginList)
    {
        return impl::doSyncCall<impl::GetAnalyticsPluginsHandler>(
            [this](impl::GetAnalyticsPluginsHandlerPtr handler)
            {
                this->getAnalyticsPlugins(handler);
            },
            outAnalyticsPluginList);
    }

    template<class TargetType, class HandlerType>
    int getAnalyticsEngines(TargetType* target, HandlerType handler)
    {
        return getAnalyticsEngines(
            std::static_pointer_cast<impl::GetAnalyticsEnginesHandler>(
                std::make_shared<
                    impl::CustomGetAnalyticsEnginesHandler<TargetType, HandlerType>>(
                        target,
                        handler)));
    }

    ErrorCode getAnalyticsEnginesSync(
        nx::vms::api::AnalyticsEngineDataList* const outAnalyticsEnginesList)
    {
        return impl::doSyncCall<impl::GetAnalyticsEnginesHandler>(
            [this](impl::GetAnalyticsEnginesHandlerPtr handler)
            {
                this->getAnalyticsEngines(handler);
            },
            outAnalyticsEnginesList);
    }


    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int save(
        const nx::vms::api::AnalyticsPluginData& resource,
        TargetType* target,
        HandlerType handler)
    {
        return save(
            resource,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int save(
        const nx::vms::api::AnalyticsEngineData& resource,
        TargetType* target,
        HandlerType handler)
    {
        return save(
            resource,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode saveSync(const nx::vms::api::AnalyticsPluginData& plugin)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [this, plugin](impl::SimpleHandlerPtr handler)
            {
                this->save(plugin, handler);
            });
    }

    ErrorCode saveSync(const nx::vms::api::AnalyticsEngineData& engine)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [this, engine](impl::SimpleHandlerPtr handler)
            {
                this->save(engine, handler);
            });
    }

    /**
     * @param handler Functor with params: (ErrorCode)
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
    virtual int getAnalyticsPlugins(impl::GetAnalyticsPluginsHandlerPtr handler) = 0;

    virtual int getAnalyticsEngines(impl::GetAnalyticsEnginesHandlerPtr handler) = 0;

    virtual int save(
        const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
        impl::SimpleHandlerPtr handler) = 0;

    virtual int save(
        const nx::vms::api::AnalyticsEngineData& analyticsPlugin,
        impl::SimpleHandlerPtr handler) = 0;

    virtual int removeAnalyticsPlugin(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

    virtual int removeAnalyticsEngine(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;
};

using AbstractAnalyticsManagerPtr = std::shared_ptr<AbstractAnalyticsManager>;

} // namespace ec2
