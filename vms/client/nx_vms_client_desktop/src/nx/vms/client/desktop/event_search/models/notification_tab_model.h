#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/models/concatenation_list_model.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class NotificationListModel;
class SystemHealthListModel;
class ProgressListModel;

class NotificationTabModel: public ConcatenationListModel
{
    Q_OBJECT
    using base_type = ConcatenationListModel;

public:
    explicit NotificationTabModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~NotificationTabModel() override;

    NotificationListModel* notificationsModel() const;
    SystemHealthListModel* systemHealthModel() const;
    ProgressListModel* progressModel() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
