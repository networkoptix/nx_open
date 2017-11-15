#pragma once

#include "../event_search_widget.h"

#include <ui/workbench/workbench_context_aware.h>

class QAbstractProxyModel;

namespace nx {
namespace client {
namespace desktop {

class EventRibbon;
class AbstractEventListModel;

namespace detail {

class EventSearchWidgetPrivateBase:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    EventSearchWidgetPrivateBase(QWidget* q);
    virtual ~EventSearchWidgetPrivateBase() override;

protected:
    void connectEventRibbonToModel(
        EventRibbon* ribbon,
        AbstractEventListModel* model,
        QAbstractProxyModel* proxyModel = nullptr);

protected:
    QWidget* const m_headerWidget = nullptr;
    EventRibbon* const m_ribbon = nullptr;
};

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx
