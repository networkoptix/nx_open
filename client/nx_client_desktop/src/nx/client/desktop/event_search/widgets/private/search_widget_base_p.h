#pragma once

#include "../event_search_widget.h"

#include <ui/workbench/workbench_context_aware.h>

class QAbstractListModel;

namespace nx {
namespace client {
namespace desktop {

class EventRibbon;

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
    void setModel(QAbstractListModel* model);

protected:
    QWidget* const m_headerWidget = nullptr;
    EventRibbon* const m_ribbon = nullptr;
};

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx
