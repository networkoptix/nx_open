#pragma once

#include "../event_list_model.h"

namespace nx {
namespace client {
namespace desktop {

class EventListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(EventListModel* q);
    virtual ~Private() override;

private:
    EventListModel* const q = nullptr;
};

} // namespace
} // namespace client
} // namespace nx
