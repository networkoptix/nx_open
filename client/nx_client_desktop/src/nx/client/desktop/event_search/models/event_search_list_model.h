#pragma once

#include <nx/client/desktop/event_search/models/abstract_async_search_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::client::desktop {

class EventSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit EventSearchListModel(QObject* parent = nullptr);
    virtual ~EventSearchListModel() override = default;

    vms::api::EventType selectedEventType() const;
    void setSelectedEventType(vms::api::EventType value);

    virtual bool isConstrained() const override;

private:
    class Private;
    Private* const d = nullptr;
};

} // namespace nx::client::desktop
