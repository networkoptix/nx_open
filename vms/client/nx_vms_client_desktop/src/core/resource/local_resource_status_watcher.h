#pragma once

#include <future>

#include "core/resource_management/resource_searcher.h"
#include <common/common_globals.h>

class QnLocalResourceStatusWatcher:
    public QObject
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnLocalResourceStatusWatcher(QObject* parent = nullptr);
    virtual ~QnLocalResourceStatusWatcher() override;

private:
    void onResourceAdded(const QnResourcePtr& resource);

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    struct WatchedResource
    {
        QnUuid resourceId;
        std::future<Qn::ResourceStatus> statusFuture;
        std::function<Qn::ResourceStatus()> statusEvaluator;
    };

    std::list<WatchedResource> m_watchedResources;
    std::optional<int> m_timerId;
};
