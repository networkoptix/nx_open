#pragma once

#include <nx/vms/client/desktop/event_search/models/abstract_async_search_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::client::desktop {

class EventSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit EventSearchListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~EventSearchListModel() override = default;

    vms::api::EventType selectedEventType() const;
    void setSelectedEventType(vms::api::EventType value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

private:
    class Private;
    Private* const d;
};

} // namespace nx::vms::client::desktop
