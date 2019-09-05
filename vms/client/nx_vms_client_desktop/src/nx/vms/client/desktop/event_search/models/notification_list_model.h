#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/models/event_list_model.h>

namespace nx::vms::client::desktop {

class NotificationListModel: public EventListModel
{
    Q_OBJECT
    using base_type = EventListModel;

public:
    explicit NotificationListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~NotificationListModel() override;

    int maximumCount() const;
    void setMaximumCount(int value);
    static constexpr int kDefaultMaximumCount = 100;

protected:
    virtual bool defaultAction(const QModelIndex& index) override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
