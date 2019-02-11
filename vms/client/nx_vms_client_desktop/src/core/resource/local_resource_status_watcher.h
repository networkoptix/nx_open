#pragma once

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
    std::list<std::pair<QnUuid, std::future<Qn::ResourceStatus>>> m_watchedResources;
    std::optional<int> m_timerId;
};
