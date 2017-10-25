#pragma once

#include "../event_search_widget.h"

#include <QtWidgets/QTabWidget>

#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class EventRibbon;
class UnifiedSearchListModel;

class EventSearchWidget::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    Private(EventSearchWidget* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    EventSearchWidget* q = nullptr;
    UnifiedSearchListModel* const m_model = nullptr;
    QWidget* const m_headerWidget = nullptr;
    EventRibbon* const m_eventRibbon = nullptr;

    QnVirtualCameraResourcePtr m_camera;
};

} // namespace
} // namespace client
} // namespace nx
