#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <api/server_rest_connection.h>

namespace nx {
namespace client {
namespace desktop {

class TimeSynchronizationWidgetStore;

class TimeSynchronizationWidgetWatcher:
    public QObject,
    public QnWorkbenchContextAware
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

} // namespace desktop
} // namespace client
} // namespace nx
