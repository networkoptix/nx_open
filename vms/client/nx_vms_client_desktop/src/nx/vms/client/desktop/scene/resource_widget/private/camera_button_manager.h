// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnMediaResourceWidget;
class QnScrollableItemsWidget;

namespace nx::vms::client::desktop {

class SystemContext;

/** Manages camera buttons (like software triggers, extended outputs, etc.). */
class CameraButtonManager: public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    CameraButtonManager(
        QnMediaResourceWidget* mediaResourceWidget,
        QObject* parent = nullptr);

    virtual ~CameraButtonManager() override;

    QnScrollableItemsWidget* container() const;
    QnScrollableItemsWidget* objectTrackingContainer() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
