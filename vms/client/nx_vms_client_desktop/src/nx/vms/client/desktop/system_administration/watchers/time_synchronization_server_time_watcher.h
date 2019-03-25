#pragma once

#include <chrono>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <api/server_rest_connection.h>

namespace nx::vms::client::desktop
{

using namespace std::chrono;

class TimeSynchronizationWidgetStore;

class TimeSynchronizationServerTimeWatcher:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit TimeSynchronizationServerTimeWatcher(TimeSynchronizationWidgetStore* store,
        QObject* parent = nullptr);
    virtual ~TimeSynchronizationServerTimeWatcher() override;

    milliseconds elapsedTime() const;

    void forceUpdate();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
