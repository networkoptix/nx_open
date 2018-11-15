#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <api/server_rest_connection.h>

namespace nx::client::desktop
{

class TimeSynchronizationWidgetStore;

class TimeSynchronizationWidgetWatcher:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;
    
public:
    explicit TimeSynchronizationWidgetWatcher(TimeSynchronizationWidgetStore* store,
        QObject* parent = nullptr);

public slots:
    void updateTimestamps();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::desktop