#pragma once

#include "../system_health_list_model.h"

#include <health/system_health.h>

#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class SystemHealthListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(SystemHealthListModel* q);
    virtual ~Private() override;

private:
    void addSystemHealthEvent(QnSystemHealth::MessageType message, const QVariant& params);
    void removeSystemHealthEvent(QnSystemHealth::MessageType message, const QVariant& params);

private:
    SystemHealthListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace
} // namespace client
} // namespace nx
