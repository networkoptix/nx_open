#pragma once

#include "../system_health_list_model.h"

#include <array>

#include <core/resource/resource_fwd.h>
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

    void beforeRemove(const EventData& event);

    using ExtraData = QPair<QnSystemHealth::MessageType, QnResourcePtr>;
    static ExtraData extraData(const EventData& event);

private:
    void addSystemHealthEvent(QnSystemHealth::MessageType message, const QVariant& params);
    void removeSystemHealthEvent(QnSystemHealth::MessageType message, const QVariant& params);

private:
    SystemHealthListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    std::array<QHash<QnResourcePtr, QnUuid>, QnSystemHealth::Count> m_uuidHashes;
};

} // namespace
} // namespace client
} // namespace nx
