#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <api/server_rest_connection.h>

namespace nx::vms::client::desktop
{

class TimeSynchronizationWidgetStore;

class TimeSynchronizationServerStateWatcher:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit TimeSynchronizationServerStateWatcher(TimeSynchronizationWidgetStore* store,
        QObject* parent = nullptr);
    virtual ~TimeSynchronizationServerStateWatcher() override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
