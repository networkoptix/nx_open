#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class EventTile;

class EventPanel:
    public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit EventPanel(QWidget* parent);
    explicit EventPanel(QnWorkbenchContext* context, QWidget* parent = nullptr);

    virtual ~EventPanel() override;

signals:
    void tileHovered(const QModelIndex& index, const EventTile* tile);

private:
    // We capture mouse press to cancel global context menu.
    virtual void mousePressEvent(QMouseEvent *event) override;

    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
