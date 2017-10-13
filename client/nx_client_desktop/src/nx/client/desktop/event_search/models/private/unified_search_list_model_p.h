#pragma once

#include "../unified_search_list_model.h"

#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class UnifiedSearchListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(UnifiedSearchListModel* q);
    virtual ~Private() override;

private:
    UnifiedSearchListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace
} // namespace client
} // namespace nx
