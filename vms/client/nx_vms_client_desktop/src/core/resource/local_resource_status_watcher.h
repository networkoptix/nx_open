// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
        std::future<nx::vms::api::ResourceStatus> statusFuture;
        std::function<nx::vms::api::ResourceStatus()> statusEvaluator;
    };

    std::list<WatchedResource> m_watchedResources;
    int m_timerId = 0;
};
