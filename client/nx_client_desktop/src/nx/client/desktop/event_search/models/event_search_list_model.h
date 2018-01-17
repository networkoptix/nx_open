#pragma once

#include <nx/client/desktop/event_search/models/abstract_async_search_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class EventSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit EventSearchListModel(QObject* parent = nullptr);
    virtual ~EventSearchListModel() override = default;

    vms::event::EventType selectedEventType() const;
    void setSelectedEventType(vms::event::EventType value);

private:
    class Private;
    Private* const d = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
