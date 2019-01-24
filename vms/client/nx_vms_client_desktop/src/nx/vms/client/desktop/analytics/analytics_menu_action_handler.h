#pragma once

#include <nx/vms/client/desktop/radass/radass_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnLayoutItemIndex;

namespace nx::vms::client::desktop {

enum class AnalyticsObjectsVisualizationMode;

class AnalyticsMenuActionsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    AnalyticsMenuActionsHandler(QObject* parent = nullptr);
    virtual ~AnalyticsMenuActionsHandler() override;

private:
    void handleAnalyticsObjectsModeActionAction();
    void handleItemModeChanged(const QnLayoutItemIndex& item,
        AnalyticsObjectsVisualizationMode mode);
    void handleLocalSystemIdChanged();

private:
    struct Private;
    QScopedPointer<Private> d;

};

} // namespace nx::vms::client::desktop
