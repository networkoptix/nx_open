#pragma once

#include <nx/vms/client/desktop/radass/radass_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnLayoutItemIndex;

namespace nx::vms::client::desktop {

class RadassActionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    RadassActionHandler(QObject* parent = nullptr);
    virtual ~RadassActionHandler() override;

private:
    void at_radassAction_triggered();
    void handleItemModeChanged(const QnLayoutItemIndex& item, RadassMode mode);
    void handleCurrentLayoutChanged();
    void handleLocalSystemIdChanged();
    void notifyAboutPerformanceLoss();

private:
    struct Private;
    QScopedPointer<Private> d;

};

} // namespace nx::vms::client::desktop
